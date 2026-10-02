#include "rocketplot/PlotWidget.h"

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QStyleHints>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "InteractionController.h"
#include "LegendLayout.h"
#include "Logging.h"
#include "MarkerPainter.h"
#include "Overlays.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "ViewHistory.h"
#include "core/Autoscale.h"
#include "core/AxisMapping.h"
#include "core/Occupancy.h"
#include "core/SeriesData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr QSize kSizeHint{640, 400};
constexpr QSize kMinimumSizeHint{160, 120};
// Alpha of the legend box when the theme's background comes from the widget palette.
constexpr int kLegendAlpha = 230;

}  // namespace

struct PlotWidget::Private
{
    explicit Private(PlotWidget& plot)
      : history([&plot] { return QList<PlotWidget*>{&plot}; },
                [&plot] { Q_EMIT plot.historyChanged(); })
    {
    }

    // Fields ordered by size, so the struct packs tightly.
    Axis*                                  xAxis        = nullptr;
    Axis*                                  yAxis        = nullptr;
    Axis*                                  yAxis2       = nullptr;
    Legend*                                legend       = nullptr;
    qsizetype                              colorCounter = 0;
    MarkerPainter                          markers;
    TextPainter                            text;
    std::unique_ptr<InteractionController> interaction;
    QPointer<PlotLink>                     link;
    QList<Series*>                         series;
    QString                                title;
    InputBindings                          bindings = InputBindings::defaults();
    std::optional<double>                  linkedX;  // a linked plot's crosshair
    std::optional<QPointF>                 hover;    // the pointer over the plot area
    ViewHistory                            history;  // when not linked
    Theme                                  theme = Theme::light();

    // The plot as last rendered, drawn again as long as nothing it shows changes (the legend,
    // crosshair and zoom box are drawn over it).
    QPixmap         cache;
    PlotLayout      cacheLayout;
    RenderStats     stats;
    core::Occupancy occupancy;  // where the cached rendering drew data, for a BEST legend

    // The legend as last drawn (clicks and pointing are matched against it).
    LegendLayout           legendLayout;
    double                 valueWidth = 0.0;  // the widest values while the crosshair is shown
    std::optional<QPointF> pointer;           // over the widget
    QPointer<Series>       pointed;           // the series whose legend entry the pointer is on

    ThemeMode                   themeMode = ThemeMode::SYSTEM;
    std::optional<LegendAnchor> bestAnchor;  // where a BEST legend went last
    bool                        debugOverlay = false;
    bool                        crosshair    = false;
    bool                        dirty        = true;

    // The current layout (with the link's margins).
    [[nodiscard]] PlotLayout layout(const PlotWidget& plot)
    {
        LayoutConstraints constraints;
        const PlotLink*   current = link.data();
        if (current != nullptr && current->alignsMargins())
        {
            const auto [left, right] = current->alignedMargins();
            constraints              = {.minLeft = left, .minRight = right};
        }
        return layoutPlot(plot, QRectF(plot.rect()), plot.font(), plot.devicePixelRatioF(), text,
                          constraints);
    }

    // The layout on screen: the cached one while it is current.
    [[nodiscard]] PlotLayout shownLayout(const PlotWidget& plot)
    {
        if (!dirty && cacheLayout.bounds == QRectF(plot.rect()) &&
            cacheLayout.devicePixelRatio == plot.devicePixelRatioF())
        {
            return cacheLayout;
        }
        return layout(plot);
    }
};

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent), m_impl(std::make_unique<Private>(*this))
{
    m_impl->xAxis  = new Axis(Qt::Horizontal, false, this);
    m_impl->yAxis  = new Axis(Qt::Vertical, false, this);
    m_impl->yAxis2 = new Axis(Qt::Vertical, true, this);
    m_impl->legend = new Legend(this);
    m_impl->interaction =
        std::make_unique<InteractionController>(*this, [this] { return m_impl->layout(*this); });
    m_impl->debugOverlay = qEnvironmentVariableIntValue("ROCKETPLOT_DEBUG_OVERLAY") == 1;
    setMouseTracking(true);  // the crosshair and the legend follow the pointer

    for (const Axis* axis : {m_impl->xAxis, m_impl->yAxis, m_impl->yAxis2})
    {
        connect(axis, &Axis::changed, this, [this] { invalidate(); });
        connect(axis, &Axis::rangeChanged, this, &PlotWidget::viewChanged);
        connect(axis, &Axis::rangeChanged, this, [this] {
            if (m_impl->hover)
            {
                Q_EMIT crosshairMoved();  // other data is under it now
            }
        });
        connect(axis, &Axis::fitNeeded, this, [this] { applyAutoscale(); });
    }
    // The y axes may fit what's visible in x; a linked plot follows this one's x axis (and
    // crosshair).
    connect(m_impl->xAxis, &Axis::rangeChanged, this, [this] {
        refitY();
        if (PlotLink* current = m_impl->link.data(); current != nullptr)
        {
            current->syncFrom(this);
        }
        if (m_impl->hover)
        {
            syncCrosshair();  // a new x under the pointer
        }
    });
    connect(m_impl->xAxis, &Axis::autoscaleChanged, this, [this] {
        if (PlotLink* current = m_impl->link.data(); current != nullptr)
        {
            current->syncFrom(this);
        }
    });
    connect(m_impl->legend, &Legend::changed, this, [this] {
        // The legend is drawn over the cached rendering, which has the data a BEST legend avoids
        // only if it was rendered for one.
        if (m_impl->legend->anchor() == LegendAnchor::BEST && !m_impl->occupancy.isRecorded())
        {
            markDirty();
        }
        else
        {
            update();
        }
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { updateSystemTheme(); });

    setAttribute(Qt::WA_OpaquePaintEvent);  // paintEvent fills every pixel
    setAttribute(Qt::WA_AcceptTouchEvents);
    updateSystemTheme();
}

PlotWidget::~PlotWidget() = default;

// Series
// --------------------------------------------------------------------------------------------------------

template <class SeriesType>
SeriesType* PlotWidget::adopt(SeriesType* series, const QString& name)
{
    series->m_colorIndex = nextColorIndex();
    series->setName(name);
    m_impl->series.append(series);
    connect(series, &Series::dataChanged, this, &PlotWidget::seriesDataChanged);
    connect(series, &Series::changed, this, &PlotWidget::seriesStyleChanged);
    applyAutoscale();
    invalidate();
    Q_EMIT seriesAdded(series);
    return series;
}

qsizetype PlotWidget::nextColorIndex()
{
    // A series keeps its color for life: removing another one doesn't repaint it.
    return m_impl->colorCounter++;
}

LineSeries* PlotWidget::addLine(std::vector<double>&& x, std::vector<double>&& y,
                                const QString& name)
{
    auto series = std::unique_ptr<LineSeries>(new LineSeries(this));
    series->setData(std::move(x), std::move(y));  // may throw: then the series is deleted
    return adopt(series.release(), name);
}

LineSeries* PlotWidget::addLine(UniformX x, std::vector<double>&& y, const QString& name)
{
    auto series = std::unique_ptr<LineSeries>(new LineSeries(this));
    series->setData(x, std::move(y));
    return adopt(series.release(), name);
}

LineSeries* PlotWidget::addLineView(std::span<const double> x, std::span<const double> y,
                                    const QString& name)
{
    auto series = std::unique_ptr<LineSeries>(new LineSeries(this));
    series->setDataView(x, y);
    return adopt(series.release(), name);
}

LineSeries* PlotWidget::addLineView(UniformX x, std::span<const double> y, const QString& name)
{
    auto series = std::unique_ptr<LineSeries>(new LineSeries(this));
    series->setDataView(x, y);
    return adopt(series.release(), name);
}

ScatterSeries* PlotWidget::addScatter(std::vector<double>&& x, std::vector<double>&& y,
                                      const QString& name)
{
    auto series = std::unique_ptr<ScatterSeries>(new ScatterSeries(this));
    series->setData(std::move(x), std::move(y));
    return adopt(series.release(), name);
}

ScatterSeries* PlotWidget::addScatter(UniformX x, std::vector<double>&& y, const QString& name)
{
    auto series = std::unique_ptr<ScatterSeries>(new ScatterSeries(this));
    series->setData(x, std::move(y));
    return adopt(series.release(), name);
}

ScatterSeries* PlotWidget::addScatterView(std::span<const double> x, std::span<const double> y,
                                          const QString& name)
{
    auto series = std::unique_ptr<ScatterSeries>(new ScatterSeries(this));
    series->setDataView(x, y);
    return adopt(series.release(), name);
}

ScatterSeries* PlotWidget::addScatterView(UniformX x, std::span<const double> y,
                                          const QString& name)
{
    auto series = std::unique_ptr<ScatterSeries>(new ScatterSeries(this));
    series->setDataView(x, y);
    return adopt(series.release(), name);
}

QList<Series*> PlotWidget::series() const
{
    return m_impl->series;
}

void PlotWidget::removeSeries(Series* series)
{
    if (!m_impl->series.removeOne(series))
    {
        return;
    }
    Q_EMIT seriesRemoved(series);
    delete series;
    applyAutoscale();
    invalidate();
}

void PlotWidget::clearSeries()
{
    while (!m_impl->series.isEmpty())
    {
        removeSeries(m_impl->series.back());
    }
}

void PlotWidget::seriesDataChanged()
{
    applyAutoscale();
    invalidate();
}

void PlotWidget::seriesStyleChanged()
{
    applyAutoscale();  // visibility changes what autoscale fits
    invalidate();
}

// Axes, legend, title
// -------------------------------------------------------------------------------------------

Axis* PlotWidget::xAxis() const noexcept
{
    return m_impl->xAxis;
}

Axis* PlotWidget::yAxis() const noexcept
{
    return m_impl->yAxis;
}

Axis* PlotWidget::yAxis2() const noexcept
{
    return m_impl->yAxis2;
}

Legend* PlotWidget::legend() const noexcept
{
    return m_impl->legend;
}

PlotLink* PlotWidget::link() const noexcept
{
    return m_impl->link;
}

void PlotWidget::setLink(PlotLink* link)
{
    m_impl->link    = link;
    m_impl->linkedX = std::nullopt;
    m_impl->history.clear();  // a view history now covers other plots, or no longer does
    invalidate();
}

void PlotWidget::invalidate()
{
    markDirty();
    if (const PlotLink* current = m_impl->link.data(); current != nullptr)
    {
        for (PlotWidget* plot : current->plots())
        {
            plot->markDirty();
        }
    }
}

void PlotWidget::markDirty()
{
    m_impl->dirty = true;
    update();
}

QString PlotWidget::title() const
{
    return m_impl->title;
}

void PlotWidget::setTitle(const QString& title)
{
    if (title == m_impl->title)
    {
        return;
    }
    m_impl->title = title;
    invalidate();
    Q_EMIT titleChanged();
}

// View
// ----------------------------------------------------------------------------------------------------------

void PlotWidget::resetView()
{
    ViewHistory& views = history();
    views.beginStep();
    for (Axis* axis : {m_impl->xAxis, m_impl->yAxis, m_impl->yAxis2})
    {
        axis->applyAutoscale(true);
    }
    applyAutoscale();
    views.commitStep();
}

void PlotWidget::back()
{
    history().back();
}

void PlotWidget::forward()
{
    history().forward();
}

bool PlotWidget::canGoBack() const
{
    return history().canGoBack();
}

bool PlotWidget::canGoForward() const
{
    return history().canGoForward();
}

ViewHistory& PlotWidget::history() const
{
    if (PlotLink* current = m_impl->link.data(); current != nullptr)
    {
        return *current->m_history;
    }
    return m_impl->history;
}

// Interaction
// ---------------------------------------------------------------------------------------------------

const InputBindings& PlotWidget::inputBindings() const noexcept
{
    return m_impl->bindings;
}

void PlotWidget::setInputBindings(const InputBindings& bindings)
{
    m_impl->bindings = bindings;
}

bool PlotWidget::isCrosshairEnabled() const noexcept
{
    return m_impl->crosshair;
}

void PlotWidget::setCrosshairEnabled(bool enabled)
{
    if (enabled == m_impl->crosshair)
    {
        return;
    }
    if (!enabled)
    {
        setHoverPosition(std::nullopt);  // while still on: linked plots let go of it too
    }
    m_impl->crosshair = enabled;
    m_impl->linkedX.reset();
    if (enabled && underMouse())
    {
        setHoverPosition(mapFromGlobal(QPointF(QCursor::pos())));
    }
    update();
    Q_EMIT crosshairEnabledChanged();
}

std::optional<QPointF> PlotWidget::crosshairPosition() const
{
    if (!m_impl->crosshair)
    {
        return std::nullopt;
    }
    if (const std::optional<QPointF> hover = m_impl->hover)
    {
        const PlotLayout layout = m_impl->shownLayout(*this);
        return QPointF(layout.x.mapping.toValue(hover->x()), layout.y.mapping.toValue(hover->y()));
    }
    if (m_impl->linkedX)
    {
        return QPointF(*m_impl->linkedX, std::numeric_limits<double>::quiet_NaN());
    }
    return std::nullopt;
}

void PlotWidget::setHoverPosition(std::optional<QPointF> position)
{
    if (!m_impl->crosshair)
    {
        return;
    }
    if (position && !m_impl->shownLayout(*this).plot.contains(*position))
    {
        position.reset();
    }
    if (position == m_impl->hover)
    {
        return;
    }
    m_impl->hover = position;
    update();
    updateCursor();
    syncCrosshair();
    Q_EMIT crosshairMoved();
}

void PlotWidget::setLinkedCrosshair(std::optional<double> x)
{
    if (x == m_impl->linkedX || !m_impl->crosshair)
    {
        return;
    }
    m_impl->linkedX = x;
    if (!m_impl->hover)
    {
        update();
        Q_EMIT crosshairMoved();
    }
}

void PlotWidget::syncCrosshair()
{
    PlotLink* current = m_impl->link.data();
    if (current == nullptr || !m_impl->crosshair)
    {
        return;
    }
    std::optional<double> x;
    if (const std::optional<QPointF> hover = m_impl->hover)
    {
        x = m_impl->shownLayout(*this).x.mapping.toValue(hover->x());
    }
    current->syncCrosshair(this, x);
}

void PlotWidget::updateCursor()
{
    switch (m_impl->interaction->dragAction())
    {
        case PlotAction::PAN:
            setCursor(Qt::ClosedHandCursor);
            return;
        case PlotAction::BOX_ZOOM:
            setCursor(Qt::CrossCursor);
            return;
        default:
            break;
    }
    if (m_impl->interaction->isDraggingLegend())
    {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_impl->legend->isInteractive() && m_impl->pointer &&
        m_impl->legendLayout.box.contains(*m_impl->pointer))
    {
        // Entries are clicked; the rest of the box is a handle.
        setCursor(m_impl->pointed ? Qt::PointingHandCursor : Qt::OpenHandCursor);
        return;
    }
    if (m_impl->hover)
    {
        setCursor(Qt::CrossCursor);
    }
    else
    {
        unsetCursor();
    }
}

void PlotWidget::showContextMenu(QPoint position, QPoint globalPosition)
{
    if (const LegendEntry* entry = legendEntryAt(QPointF(position)))
    {
        showEntryMenu(entry->series, globalPosition);
        return;
    }
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(tr("Back"), this, &PlotWidget::back)->setEnabled(canGoBack());
    menu->addAction(tr("Forward"), this, &PlotWidget::forward)->setEnabled(canGoForward());
    menu->addAction(tr("Reset view"), this, &PlotWidget::resetView);
    menu->addSeparator();
    QAction* crosshair = menu->addAction(tr("Crosshair"));
    crosshair->setCheckable(true);
    crosshair->setChecked(isCrosshairEnabled());
    connect(crosshair, &QAction::toggled, this, &PlotWidget::setCrosshairEnabled);
    Q_EMIT contextMenuAboutToShow(menu, QPointF(position));
    menu->popup(globalPosition);
}

// Legend
// --------------------------------------------------------------------------------------------------------

const LegendEntry* PlotWidget::legendEntryAt(QPointF position) const
{
    if (!m_impl->legend->isInteractive())
    {
        return nullptr;
    }
    return m_impl->legendLayout.entryAt(position);
}

bool PlotWidget::isOnLegend(QPointF position) const
{
    return m_impl->legend->isInteractive() && m_impl->legendLayout.box.contains(position);
}

void PlotWidget::pointAt(std::optional<QPointF> position)
{
    m_impl->pointer             = position;
    const LegendEntry* entry    = position ? legendEntryAt(*position) : nullptr;
    Series*            series   = entry != nullptr ? entry->series : nullptr;
    const Series*      previous = m_impl->pointed.data();
    if (series != previous)
    {
        m_impl->pointed = series;
        // The highlighted series is drawn over the others, which fade.
        if ((series != nullptr && series->isVisible()) ||
            (previous != nullptr && previous->isVisible()))
        {
            markDirty();
        }
        update();
    }
    updateCursor();
}

const Series* PlotWidget::highlightedSeries() const
{
    const Series* series = m_impl->pointed.data();
    return series != nullptr && series->isVisible() && m_impl->legend->isInteractive() ? series
                                                                                       : nullptr;
}

void PlotWidget::isolateSeries(Series* series)
{
    // Showing only the one already shown alone shows them all again.
    const bool alone =
        series->isVisible() && std::ranges::none_of(m_impl->series, [series](const Series* other) {
            return other != series && other->isVisible();
        });
    for (Series* other : std::as_const(m_impl->series))
    {
        other->setVisible(alone || other == series);
    }
}

void PlotWidget::showEntryMenu(Series* series, QPoint globalPosition)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QPointer<Series> target(series);
    const auto             onTarget = [target](auto&& action) {
        return [target, action] {
            if (Series* current = target.data())
            {
                action(*current);
            }
        };
    };

    QAction* visible = menu->addAction(tr("Visible"));
    visible->setCheckable(true);
    visible->setChecked(series->isVisible());
    connect(visible, &QAction::triggered, this,
            onTarget([](Series& current) { current.setVisible(!current.isVisible()); }));
    menu->addAction(tr("Show only this"), this, onTarget([this](Series& current) {
                        for (Series* other : std::as_const(m_impl->series))
                        {
                            other->setVisible(other == &current);
                        }
                    }));
    menu->addAction(tr("Show all"), this, [this] {
        for (Series* other : std::as_const(m_impl->series))
        {
            other->setVisible(true);
        }
    });
    menu->addSeparator();

    menu->addAction(tr("Color…"), this, onTarget([this](Series& current) {
                        const QColor color =
                            QColorDialog::getColor(current.color(), this, tr("Series color"));
                        if (color.isValid())
                        {
                            current.setColor(color);
                        }
                    }));
    if (series->m_color)
    {
        menu->addAction(tr("Theme color"), this,
                        onTarget([](Series& current) { current.resetColor(); }));
    }
    if (auto* line = qobject_cast<LineSeries*>(series))
    {
        QMenu*        widths = menu->addMenu(tr("Line width"));
        const QLocale locale;
        for (const double width : {1.0, 1.5, 2.0, 3.0, 4.0})
        {
            QAction* action = widths->addAction(tr("%1 px").arg(locale.toString(width)));
            action->setCheckable(true);
            action->setChecked(line->lineWidth() == width);
            connect(action, &QAction::triggered, this, onTarget([width](Series& current) {
                        if (auto* currentLine = qobject_cast<LineSeries*>(&current))
                        {
                            currentLine->setLineWidth(width);
                        }
                    }));
        }
    }
    QMenu*     markers = menu->addMenu(tr("Marker"));
    const auto shapes  = std::to_array<std::pair<Marker, QString>>({
        {Marker::NONE, tr("None")},
        {Marker::CIRCLE, tr("Circle")},
        {Marker::SQUARE, tr("Square")},
        {Marker::DIAMOND, tr("Diamond")},
        {Marker::TRIANGLE, tr("Triangle")},
        {Marker::CROSS, tr("Cross")},
        {Marker::PLUS, tr("Plus")},
    });
    const bool scatter = qobject_cast<LineSeries*>(series) == nullptr;
    for (const auto& [shape, name] : shapes)
    {
        if (shape == Marker::NONE && scatter)
        {
            continue;  // a scatter series without markers draws nothing
        }
        QAction* action = markers->addAction(markerIcon(*series, shape), name);
        action->setCheckable(true);
        action->setChecked(series->marker() == shape);
        connect(action, &QAction::triggered, this,
                onTarget([shape](Series& current) { current.setMarker(shape); }));
    }
    menu->addSeparator();
    menu->addAction(tr("Remove"), this,
                    onTarget([this](Series& current) { removeSeries(&current); }));
    Q_EMIT m_impl->legend->entryMenuAboutToShow(menu, series);
    menu->popup(globalPosition);
}

QIcon PlotWidget::markerIcon(const Series& series, Marker shape) const
{
    constexpr int    kIconSize   = 16;
    constexpr double kMarkerSize = 10.0;
    const double     dpr         = devicePixelRatioF();
    QPixmap          pixmap(QSize(kIconSize, kIconSize) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    if (shape != Marker::NONE)
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        MarkerStyle style = markerStyle(series, m_impl->theme);
        style.shape       = shape;
        style.size        = kMarkerSize;
        style.ringWidth   = 0.0;
        MarkerPainter::draw(painter, QPointF(kIconSize / 2.0, kIconSize / 2.0), style);
    }
    return {pixmap};
}

void PlotWidget::applyAutoscale()
{
    refitX();
    refitY();
}

Range PlotWidget::xDataBounds(bool positiveOnly) const
{
    Range bounds = Range::empty();
    for (const Series* series : std::as_const(m_impl->series))
    {
        if (series->isVisible())
        {
            bounds =
                bounds.united(positiveOnly ? series->data().xPositiveBounds() : series->xBounds());
        }
    }
    return bounds;
}

void PlotWidget::refitX()
{
    Axis& axis = *m_impl->xAxis;
    if (!axis.autoscale())
    {
        return;
    }
    const core::Scale scale  = scaleOf(axis);
    const bool        log    = scale == core::Scale::LOG;
    const PlotLink*   link   = m_impl->link.data();
    const Range       bounds = link != nullptr ? link->xDataBounds(log) : xDataBounds(log);
    if (axis.autoscaleMode() == AutoscaleMode::FOLLOW_LATEST && !log)
    {
        axis.applyRange(core::followRange(bounds, axis.followWindow(), axis.autoscaleMargin()));
    }
    else
    {
        axis.applyRange(core::autoscaleRange(bounds, axis.autoscaleMargin(), scale));
    }
}

void PlotWidget::refitY()
{
    const Range xRange = m_impl->xAxis->range();
    for (Axis* axis : {m_impl->yAxis, m_impl->yAxis2})
    {
        if (!axis->autoscale())
        {
            continue;
        }
        const core::Scale scale   = scaleOf(*axis);
        const bool        log     = scale == core::Scale::LOG;
        const bool        visible = axis->autoscaleMode() != AutoscaleMode::FIT_ALL;
        Range             bounds  = Range::empty();
        for (const Series* series : std::as_const(m_impl->series))
        {
            if (!series->isVisible() || series->isOnSecondaryYAxis() != axis->isSecondary())
            {
                continue;
            }
            const core::SeriesData& seriesData = series->data();
            if (visible)
            {
                bounds = bounds.united(seriesData.yBoundsWithin(xRange, log));
            }
            else
            {
                bounds = bounds.united(log ? seriesData.yPositiveBounds() : seriesData.yBounds());
            }
        }
        // Fitting what's visible: with nothing in view, stay put rather than jump to a default.
        if (visible && !bounds.isValid())
        {
            continue;
        }
        axis->applyRange(core::autoscaleRange(bounds, axis->autoscaleMargin(), scale));
    }
}

// Appearance
// ----------------------------------------------------------------------------------------------------

ThemeMode PlotWidget::themeMode() const noexcept
{
    return m_impl->themeMode;
}

void PlotWidget::setThemeMode(ThemeMode mode)
{
    if (mode == m_impl->themeMode)
    {
        return;
    }
    m_impl->themeMode = mode;
    switch (mode)
    {
        case ThemeMode::SYSTEM:
            updateSystemTheme();
            break;
        case ThemeMode::LIGHT:
            m_impl->theme = Theme::light();
            break;
        case ThemeMode::DARK:
            m_impl->theme = Theme::dark();
            break;
        case ThemeMode::CUSTOM:
            break;
    }
    invalidate();
    Q_EMIT themeChanged();
}

const Theme& PlotWidget::theme() const noexcept
{
    return m_impl->theme;
}

void PlotWidget::setTheme(const Theme& theme)
{
    if (m_impl->themeMode == ThemeMode::CUSTOM && theme == m_impl->theme)
    {
        return;
    }
    m_impl->themeMode = ThemeMode::CUSTOM;
    m_impl->theme     = theme;
    invalidate();
    Q_EMIT themeChanged();
}

void PlotWidget::updateSystemTheme()
{
    if (m_impl->themeMode != ThemeMode::SYSTEM)
    {
        return;
    }
    // The palette says what the host application looks like, whether it follows the OS or sets its
    // own colors; the plot sits on its Base color, like a view or text field.
    const QColor base      = palette().color(QPalette::Base);
    Theme        theme     = base.lightnessF() < 0.5F ? Theme::dark() : Theme::light();
    theme.background       = base;
    theme.legendBackground = QColor(base.red(), base.green(), base.blue(), kLegendAlpha);
    if (theme == m_impl->theme)
    {
        return;
    }
    m_impl->theme = std::move(theme);
    invalidate();
    Q_EMIT themeChanged();
}

bool PlotWidget::debugOverlay() const noexcept
{
    return m_impl->debugOverlay;
}

void PlotWidget::setDebugOverlay(bool enabled)
{
    if (enabled == m_impl->debugOverlay)
    {
        return;
    }
    m_impl->debugOverlay = enabled;
    update();
    Q_EMIT debugOverlayChanged();
}

// Geometry
// ------------------------------------------------------------------------------------------------------

QRectF PlotWidget::plotArea() const
{
    return m_impl->layout(*this).plot;
}

QRectF PlotWidget::legendArea() const
{
    return m_impl->legendLayout.box;
}

QPointF PlotWidget::mapToData(QPointF widgetPosition, const Axis* yAxis) const
{
    const PlotLayout  layout = m_impl->layout(*this);
    const AxisLayout& y      = yAxis == m_impl->yAxis2 ? layout.y2 : layout.y;
    return {layout.x.mapping.toValue(widgetPosition.x()), y.mapping.toValue(widgetPosition.y())};
}

QPointF PlotWidget::mapFromData(QPointF dataPosition, const Axis* yAxis) const
{
    const PlotLayout  layout = m_impl->layout(*this);
    const AxisLayout& y      = yAxis == m_impl->yAxis2 ? layout.y2 : layout.y;
    return {layout.x.mapping.toPixel(dataPosition.x()), y.mapping.toPixel(dataPosition.y())};
}

std::pair<double, double> PlotWidget::naturalMargins() const
{
    const PlotLayout layout = layoutPlot(*this, QRectF(rect()), font(), devicePixelRatioF(),
                                         m_impl->text, LayoutConstraints{});
    return {layout.naturalLeft, layout.naturalRight};
}

QSize PlotWidget::sizeHint() const
{
    return kSizeHint;
}

QSize PlotWidget::minimumSizeHint() const
{
    return kMinimumSizeHint;
}

// Events
// --------------------------------------------------------------------------------------------------------

bool PlotWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            if (const auto* touch = dynamic_cast<QTouchEvent*>(event);
                touch != nullptr && m_impl->interaction->touch(*touch))
            {
                event->accept();
                return true;
            }
            break;
        case QEvent::NativeGesture:
            if (const auto* gesture = dynamic_cast<QNativeGestureEvent*>(event);
                gesture != nullptr && m_impl->interaction->nativeGesture(*gesture))
            {
                event->accept();
                return true;
            }
            break;
        case QEvent::Show:
        case QEvent::Hide:
            invalidate();  // linked plots line up with the plots shown
            break;
        default:
            break;
    }
    return QWidget::event(event);
}

void PlotWidget::renderCache()
{
    QElapsedTimer timer;
    timer.start();
    const double dpr = devicePixelRatioF();
    const QSize  pixels(static_cast<int>(std::ceil(width() * dpr)),
                        static_cast<int>(std::ceil(height() * dpr)));
    if (m_impl->cache.size() != pixels)
    {
        m_impl->cache = QPixmap(pixels);
    }
    m_impl->cache.setDevicePixelRatio(dpr);
    m_impl->cacheLayout = m_impl->layout(*this);
    RenderStats stats;
    {
        // A BEST legend needs to know where the data is drawn.
        const bool best = m_impl->legend->anchor() == LegendAnchor::BEST;
        if (!best)
        {
            m_impl->occupancy.clear();
        }
        QPainter     painter(&m_impl->cache);
        PlotRenderer renderer(*this, m_impl->cacheLayout, m_impl->markers, m_impl->text);
        renderer.render(painter, stats,
                        {
                            .highlighted = highlightedSeries(),
                            .occupancy   = best ? &m_impl->occupancy : nullptr,
                        });
    }
    stats.milliseconds = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    m_impl->stats      = std::move(stats);
    m_impl->dirty      = false;
    qCDebug(lcRender) << "frame" << m_impl->stats.milliseconds << "ms,"
                      << m_impl->stats.series.size() << "series";
}

void PlotWidget::paintEvent(QPaintEvent* /*event*/)
{
    if (m_impl->dirty || m_impl->cacheLayout.bounds != QRectF(rect()) ||
        m_impl->cache.devicePixelRatio() != devicePixelRatioF())
    {
        renderCache();
    }
    if (m_impl->hover)
    {
        syncCrosshair();  // linked plots show the x under the pointer as drawn
    }
    QPainter painter(this);
    painter.drawPixmap(QPointF(), m_impl->cache);
    const PlotLayout& layout = m_impl->cacheLayout;
    if (layout.valid && m_impl->crosshair && (m_impl->hover || m_impl->linkedX))
    {
        drawCrosshair(painter, *this, layout, m_impl->hover, m_impl->linkedX);
    }

    // The legend, with the values at the crosshair.
    const std::optional<QPointF> crosshair = crosshairPosition();
    if (!crosshair)
    {
        m_impl->valueWidth = 0.0;
    }
    m_impl->legendLayout =
        layoutLegend(*this, layout, m_impl->text,
                     {
                         .crosshairX    = crosshair ? std::optional(crosshair->x()) : std::nullopt,
                         .minValueWidth = m_impl->valueWidth,
                         .occupancy     = &m_impl->occupancy,
                         .previousBest  = m_impl->bestAnchor,
                     });
    m_impl->valueWidth = std::max(m_impl->valueWidth, m_impl->legendLayout.valueWidth);
    if (m_impl->legend->anchor() == LegendAnchor::BEST && m_impl->legendLayout.isShown())
    {
        m_impl->bestAnchor = m_impl->legendLayout.anchor;
    }
    else
    {
        m_impl->bestAnchor.reset();
    }
    drawLegend(painter, *this, layout, m_impl->legendLayout, m_impl->text,
               m_impl->legend->isInteractive() ? m_impl->pointed.data() : nullptr);

    if (const std::optional<QRectF> box = m_impl->interaction->zoomBox(); box && layout.valid)
    {
        drawZoomBox(painter, layout, m_impl->theme, *box);
    }
    if (m_impl->debugOverlay)
    {
        drawDebugOverlay(painter, layout, m_impl->stats, m_impl->legendLayout.box);
    }
}

void PlotWidget::resizeEvent(QResizeEvent* event)
{
    invalidate();
    QWidget::resizeEvent(event);
}

void PlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_impl->interaction->mousePress(*event))
    {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_impl->interaction->mouseMove(*event))
    {
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void PlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_impl->interaction->mouseRelease(*event))
    {
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PlotWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_impl->interaction->mouseDoubleClick(*event))
    {
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PlotWidget::wheelEvent(QWheelEvent* event)
{
    if (m_impl->interaction->wheel(*event))
    {
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void PlotWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_impl->interaction->contextMenu(*event))
    {
        showContextMenu(event->pos(), event->globalPos());
    }
    event->accept();
}

void PlotWidget::leaveEvent(QEvent* event)
{
    m_impl->interaction->leave();
    pointAt(std::nullopt);
    QWidget::leaveEvent(event);
}

void PlotWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    {
        updateSystemTheme();
    }
    else if (event->type() == QEvent::FontChange)
    {
        invalidate();
    }
    QWidget::changeEvent(event);
}

}  // namespace rocketplot
