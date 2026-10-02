#include "rocketplot/PlotWidget.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStyleHints>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "InteractionController.h"
#include "Logging.h"
#include "MarkerPainter.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "core/Autoscale.h"
#include "core/AxisMapping.h"
#include "core/SeriesData.h"
#include "rocketplot/Axis.h"
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
    Axis*                                  xAxis  = nullptr;
    Axis*                                  yAxis  = nullptr;
    Axis*                                  yAxis2 = nullptr;
    Legend*                                legend = nullptr;
    QList<Series*>                         series;
    QString                                title;
    ThemeMode                              themeMode    = ThemeMode::SYSTEM;
    Theme                                  theme        = Theme::light();
    bool                                   debugOverlay = false;
    qsizetype                              colorCounter = 0;
    MarkerPainter                          markers;
    TextPainter                            text;
    QPointer<PlotLink>                     link;
    std::unique_ptr<InteractionController> interaction;

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
};

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent), m_impl(std::make_unique<Private>())
{
    m_impl->xAxis  = new Axis(Qt::Horizontal, false, this);
    m_impl->yAxis  = new Axis(Qt::Vertical, false, this);
    m_impl->yAxis2 = new Axis(Qt::Vertical, true, this);
    m_impl->legend = new Legend(this);
    m_impl->interaction =
        std::make_unique<InteractionController>(*this, [this] { return m_impl->layout(*this); });
    m_impl->debugOverlay = qEnvironmentVariableIntValue("ROCKETPLOT_DEBUG_OVERLAY") == 1;

    for (const Axis* axis : {m_impl->xAxis, m_impl->yAxis, m_impl->yAxis2})
    {
        connect(axis, &Axis::changed, this, [this] { update(); });
        connect(axis, &Axis::rangeChanged, this, &PlotWidget::viewChanged);
        connect(axis, &Axis::fitNeeded, this, [this] { applyAutoscale(); });
    }
    // The y axes may fit what's visible in x; a linked plot follows this one's x axis.
    connect(m_impl->xAxis, &Axis::rangeChanged, this, [this] {
        refitY();
        if (PlotLink* current = m_impl->link.data(); current != nullptr)
        {
            current->syncFrom(this);
        }
    });
    connect(m_impl->xAxis, &Axis::autoscaleChanged, this, [this] {
        if (PlotLink* current = m_impl->link.data(); current != nullptr)
        {
            current->syncFrom(this);
        }
    });
    connect(m_impl->legend, &Legend::changed, this, [this] { update(); });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { updateSystemTheme(); });

    setAttribute(Qt::WA_OpaquePaintEvent);  // paintEvent fills every pixel
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
    update();
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
    update();
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
    update();
}

void PlotWidget::seriesStyleChanged()
{
    applyAutoscale();  // visibility changes what autoscale fits
    update();
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
    m_impl->link = link;
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
    update();
    Q_EMIT titleChanged();
}

void PlotWidget::resetView()
{
    for (Axis* axis : {m_impl->xAxis, m_impl->yAxis, m_impl->yAxis2})
    {
        axis->applyAutoscale(true);
    }
    applyAutoscale();
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
    update();
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
    update();
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
    update();
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

void PlotWidget::paintEvent(QPaintEvent* /*event*/)
{
    QElapsedTimer timer;
    timer.start();
    QPainter         painter(this);
    const PlotLayout layout = m_impl->layout(*this);
    RenderStats      stats;
    PlotRenderer     renderer(*this, layout, m_impl->markers, m_impl->text);
    renderer.render(painter, stats);
    stats.milliseconds = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    if (m_impl->debugOverlay)
    {
        drawDebugOverlay(painter, layout, stats);
    }
    qCDebug(lcRender) << "frame" << stats.milliseconds << "ms," << stats.series.size() << "series";
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

void PlotWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    {
        updateSystemTheme();
    }
    else if (event->type() == QEvent::FontChange)
    {
        update();
    }
    QWidget::changeEvent(event);
}

}  // namespace rocketplot
