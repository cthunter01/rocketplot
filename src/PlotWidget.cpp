#include "rocketplot/PlotWidget.h"

#include <QAction>
#include <QActionGroup>
#include <QChar>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QIODevice>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QList>
#include <QLocale>
#include <QMarginsF>
#include <QMenu>
#include <QMessageBox>
#include <QMetaMethod>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPaintDevice>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QSize>
#include <QString>
#include <QStyleHints>
#include <QToolTip>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "AnnotationPainter.h"
#include "FlagLayout.h"
#include "InteractionController.h"
#include "LegendLayout.h"
#include "Logging.h"
#include "MarkerPainter.h"
#include "Overlays.h"
#include "PlotExport.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "PlotState.h"
#include "TextPainter.h"
#include "ViewHistory.h"
#include "core/Aspect.h"
#include "core/Autoscale.h"
#include "core/AxisMapping.h"
#include "core/CsvWriter.h"
#include "core/Decimator.h"
#include "core/NearestPoint.h"
#include "core/Occupancy.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/BarSeries.h"
#include "rocketplot/BoxSeries.h"
#include "rocketplot/BoxStatistics.h"
#include "rocketplot/ColorScale.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/ExportOptions.h"
#include "rocketplot/HistogramSeries.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/Range.h"
#include "rocketplot/RectAnnotation.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Series.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
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

// The resolution at which an image has one pixel per device-independent pixel.
constexpr double kBaseDpi = 96.0;

// How near the pointer a data point has to be for a snapping crosshair to go to it, in
// device-independent pixels.
constexpr double kSnapReach = 20.0;
// A point on the edge of the plot area may come out a rounding error outside it: one this far
// outside still counts as inside.
constexpr double kEdgeTolerance = 0.5;

// Headroom for event flags: the gap left between the flags and the data, the share of the plot
// area it may take at most, the layouts finding it may take, and the change in share that counts.
constexpr double kHeadroomGap      = 4.0;
constexpr double kMaxHeadroomShare = 0.5;
constexpr int    kHeadroomPasses   = 3;
// Layouts at most to settle the aspect ratio (the y labels' width depends a little on the ranges),
// and how far from it ranges may be and still count as to scale.
constexpr int    kAspectRounds    = 6;
constexpr double kAspectTolerance = 1e-9;
constexpr double kHeadroomEpsilon = 1e-9;

// Draws with another theme for as long as it lives, if given one: exports can ask for a theme of
// their own. The plot is not told and doesn't repaint.
class ThemeOverride
{
public:
    ThemeOverride(Theme& target, const std::optional<Theme>& theme) : m_target(&target)
    {
        if (theme)
        {
            m_saved = std::exchange(target, *theme);
        }
    }
    ~ThemeOverride()
    {
        if (m_saved)
        {
            *m_target = std::move(*m_saved);
        }
    }
    ThemeOverride(const ThemeOverride&)            = delete;
    ThemeOverride& operator=(const ThemeOverride&) = delete;
    ThemeOverride(ThemeOverride&&)                 = delete;
    ThemeOverride& operator=(ThemeOverride&&)      = delete;

private:
    Theme*               m_target;
    std::optional<Theme> m_saved;
};

// Guarded pointers to @p series, for actions that run later, when some of them may be gone.
QList<QPointer<Series>> pointersTo(const std::vector<Series*>& series)
{
    QList<QPointer<Series>> pointers;
    for (Series* one : series)
    {
        pointers.append(one);
    }
    return pointers;
}

// What runs @p action on each of @p targets that is still there.
template <class Action>
auto forEachOf(const QList<QPointer<Series>>& targets, Action action)
{
    return [targets, action] {
        for (const QPointer<Series>& target : targets)
        {
            if (Series* current = target.data())
            {
                action(*current);
            }
        }
    };
}

// The part of @p range a log axis can show: its positive ends.
Range positivePart(Range range)
{
    if (!(range.max > 0.0))
    {
        return Range::empty();
    }
    return {.min = range.min > 0.0 ? range.min : range.max, .max = range.max};
}

// The larger of each margin of @p a and @p b.
LayoutConstraints widest(const LayoutConstraints& a, const LayoutConstraints& b)
{
    return {
        .minLeft   = std::max(a.minLeft, b.minLeft),
        .minRight  = std::max(a.minRight, b.minRight),
        .minTop    = std::max(a.minTop, b.minTop),
        .minBottom = std::max(a.minBottom, b.minBottom),
    };
}

// Whether @p x and @p y are to scale at @p ratio in @p plot, but for rounding errors.
bool isToScale(Range x, Range y, const QRectF& plot, double ratio)
{
    const double xPerPixel = x.span() / plot.width();
    const double yPerPixel = y.span() / plot.height();
    return std::abs(xPerPixel - (ratio * yPerPixel)) <= kAspectTolerance * xPerPixel;
}

// The margins around @p plot, a plot area laid out in @p bounds.
LayoutConstraints marginsOf(const QRectF& bounds, const QRectF& plot)
{
    return {
        .minLeft   = plot.left() - bounds.left(),
        .minRight  = bounds.right() - plot.right(),
        .minTop    = plot.top() - bounds.top(),
        .minBottom = bounds.bottom() - plot.bottom(),
    };
}

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
    ColorScale*                            colorScale   = nullptr;
    qsizetype                              colorCounter = 0;
    MarkerPainter                          markers;
    TextPainter                            text;
    std::unique_ptr<InteractionController> interaction;
    QPointer<PlotLink>                     link;
    QPointer<PlotGrid>                     grid;
    QList<Series*>                         series;
    QList<Annotation*>                     annotations;
    QString                                title;
    QString                                subtitle;
    InputBindings                          bindings = InputBindings::defaults();
    std::optional<double>                  linkedX;  // a linked plot's crosshair
    std::optional<QPointF>                 hover;    // the pointer over the plot area
    // The data point the crosshair is on (see PlotWidget::crosshairPoint()): out of date once
    // pointStale is set.
    std::optional<CrosshairPoint> point;
    ViewHistory                   history;  // when not linked
    Theme                         theme = Theme::light();

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
    QPointer<QObject>      pointed;           // the key of the legend entry the pointer is on
    // What the annotations of the cached rendering drew that the pointer can be on, and the
    // annotation the pointer is on.
    std::vector<AnnotationHit> annotationHits;
    QPointer<Annotation>       hovered;

    ThemeMode                   themeMode = ThemeMode::SYSTEM;
    std::optional<LegendAnchor> bestAnchor;  // where a BEST legend went last
    // The margins the plot's labels need on screen, kept until something it shows changes: the
    // plots of a link or a grid each ask all the others for theirs, on every layout.
    struct NaturalMargins
    {
        QMarginsF margins;
        QSize     size;
        double    devicePixelRatio = 0.0;
    };
    std::optional<NaturalMargins>  naturalMargins;
    CrosshairMode                  crosshairMode = CrosshairMode::FREE;
    std::optional<Qt::CursorShape> plotCursor;  // over the plot area, once set
    FlagPlacement                  flagPlacement   = FlagPlacement::OVER_DATA;
    Qt::Orientation                flagOrientation = Qt::Horizontal;
    double                         headroomShare   = 0.0;  // left free above autoscaled y data
    bool                           debugOverlay    = false;
    bool                           crosshair       = false;
    bool                           zeroAligned     = false;  // the y axes' zeros line up
    double                         aspectRatio     = 0.0;    // 0: free
    // What the x and y axes were last asked to show (not by the aspect ratio): the ranges it
    // widens.
    std::optional<Range> wantedX;
    std::optional<Range> wantedY;
    // The margins the plot area keeps at the ranges the aspect ratio settled on (aspectRanges()).
    LayoutConstraints aspectMargins;
    bool              settlingAspect = false;
    bool              dirty          = true;
    bool              pointStale     = true;

    // Whether the crosshair is shown at the pointer and goes to the data: a change to the data
    // can then move it.
    [[nodiscard]] bool crosshairFollowsData() const
    {
        return crosshair && crosshairMode != CrosshairMode::FREE && hover.has_value();
    }

    // The axes under the names a saved state has them by.
    using NamedAxis = std::pair<QLatin1String, Axis*>;
    [[nodiscard]] std::array<NamedAxis, 3> axesByKey() const
    {
        return {
            NamedAxis{QLatin1String("xAxis"), xAxis},
            NamedAxis{QLatin1String("yAxis"), yAxis},
            NamedAxis{QLatin1String("yAxis2"), yAxis2},
        };
    }

    // The margins the plot keeps to: those it lines up with, and under a fixed aspect ratio those
    // its ranges were settled in.
    [[nodiscard]] LayoutConstraints constraints(const PlotWidget& plot) const
    {
        if (!plot.hasFixedAspect())
        {
            return alignedConstraints(plot);
        }
        return widest(alignedConstraints(plot), aspectMargins);
    }

    // The margins the plot lines up with: its grid's or its link's.
    [[nodiscard]] LayoutConstraints alignedConstraints(const PlotWidget& plot) const
    {
        const PlotLink* current = link.data();
        if (const PlotGrid* inGrid = grid.data(); inGrid != nullptr)
        {
            return inGrid->constraintsFor(&plot);
        }
        if (current != nullptr && current->alignsMargins())
        {
            const auto [left, right] = current->alignedMargins();
            return {.minLeft = left, .minRight = right};
        }
        return {};
    }

    // The layout in @p bounds (with the link's margins).
    [[nodiscard]] PlotLayout layoutIn(const PlotWidget& plot, const QRectF& bounds,
                                      double devicePixelRatio, const RangeOverrides& overrides = {})
    {
        return layoutPlot(plot, bounds, plot.font(), devicePixelRatio, text, constraints(plot),
                          overrides);
    }

    // The widget's current layout.
    [[nodiscard]] PlotLayout layout(const PlotWidget& plot)
    {
        return layoutIn(plot, QRectF(plot.rect()), plot.devicePixelRatioF());
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
    m_impl->xAxis      = new Axis(Qt::Horizontal, false, this);
    m_impl->yAxis      = new Axis(Qt::Vertical, false, this);
    m_impl->yAxis2     = new Axis(Qt::Vertical, true, this);
    m_impl->legend     = new Legend(this);
    m_impl->colorScale = new ColorScale(this);
    // A new setting or range: drawn again; autoscale turned on: fitted now.
    connect(m_impl->colorScale, &ColorScale::changed, this, [this] {
        refitColorScale();
        invalidate();
    });
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
    // What the x and y axes are asked to show, unless the aspect ratio is fitting them.
    connect(m_impl->xAxis, &Axis::rangeChanged, this, [this] {
        if (!m_impl->settlingAspect)
        {
            m_impl->wantedX = m_impl->xAxis->range();
        }
    });
    connect(m_impl->yAxis, &Axis::rangeChanged, this, [this] {
        if (!m_impl->settlingAspect)
        {
            m_impl->wantedY = m_impl->yAxis->range();
        }
    });
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
        // A legend outside the plot area, or one that was, takes room from it: a new layout.
        if (isOutside(m_impl->legend->anchor()) || !m_impl->cacheLayout.legend.box.isNull())
        {
            invalidate();
            return;
        }
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
    if (m_impl->crosshairFollowsData())
    {
        Q_EMIT crosshairMoved();  // the new series may be the nearest
    }
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

BarSeries* PlotWidget::addBars(std::vector<double>&& edges, std::vector<double>&& heights,
                               const QString& name)
{
    auto series = std::unique_ptr<BarSeries>(new BarSeries(this));
    series->setBars(std::move(edges), std::move(heights));
    return adopt(series.release(), name);
}

BoxSeries* PlotWidget::addBox(const BoxStatistics& statistics, double position, const QString& name)
{
    auto series = std::unique_ptr<BoxSeries>(new BoxSeries(this));
    series->setPosition(position);
    series->setStatistics(statistics);
    return adopt(series.release(), name);
}

HistogramSeries* PlotWidget::addHistogram(std::vector<double>&& values, const QString& name,
                                          int bins, Range binRange,
                                          HistogramNormalization normalization)
{
    // The settings first, while there are no values to count.
    auto series = std::unique_ptr<HistogramSeries>(new HistogramSeries(this));
    series->setBinCount(bins);
    if (binRange.isValid())
    {
        series->setBinRange(binRange);
    }
    series->setNormalization(normalization);
    series->setValues(std::move(values));
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
    m_impl->point.reset();  // the crosshair may be on a point of this series
    m_impl->pointStale = true;
    Q_EMIT seriesRemoved(series);
    delete series;
    if (m_impl->series.isEmpty())
    {
        m_impl->colorCounter = 0;  // nothing left to tell apart from: the colors start over
    }
    applyAutoscale();
    invalidate();
    if (m_impl->crosshairFollowsData())
    {
        Q_EMIT crosshairMoved();
    }
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
    if (m_impl->crosshairFollowsData())
    {
        Q_EMIT crosshairMoved();  // other points are near the pointer now
    }
}

void PlotWidget::seriesStyleChanged()
{
    applyAutoscale();  // visibility changes what autoscale fits
    invalidate();
    if (m_impl->crosshairFollowsData())
    {
        Q_EMIT crosshairMoved();  // a series was hidden or shown, or changed its y axis
    }
}

// Annotations
// ---------------------------------------------------------------------------------------------------

template <class AnnotationType>
AnnotationType* PlotWidget::adoptAnnotation(AnnotationType* annotation)
{
    m_impl->annotations.append(annotation);
    connect(annotation, &Annotation::changed, this, [this] {
        applyAutoscale();  // it may be one that autoscale makes room for
        invalidate();
    });
    invalidate();
    Q_EMIT annotationAdded(annotation);
    return annotation;
}

ReferenceLine* PlotWidget::addHorizontalLine(double y, const QString& label)
{
    auto* line = new ReferenceLine(this, Qt::Horizontal, y);
    line->setLabel(label);
    return adoptAnnotation(line);
}

ReferenceLine* PlotWidget::addVerticalLine(double x, const QString& label)
{
    auto* line = new ReferenceLine(this, Qt::Vertical, x);
    line->setLabel(label);
    return adoptAnnotation(line);
}

ShadedSpan* PlotWidget::addHorizontalSpan(double yMin, double yMax, const QString& label)
{
    auto* span = new ShadedSpan(this, Qt::Horizontal, Range{.min = yMin, .max = yMax});
    span->setLabel(label);
    return adoptAnnotation(span);
}

ShadedSpan* PlotWidget::addVerticalSpan(double xMin, double xMax, const QString& label)
{
    auto* span = new ShadedSpan(this, Qt::Vertical, Range{.min = xMin, .max = xMax});
    span->setLabel(label);
    return adoptAnnotation(span);
}

TextAnnotation* PlotWidget::addText(double x, double y, const QString& text)
{
    return adoptAnnotation(new TextAnnotation(this, QPointF(x, y), text));
}

EventMarker* PlotWidget::addEvent(double x, const QString& label)
{
    return adoptAnnotation(new EventMarker(this, x, label));
}

IconAnnotation* PlotWidget::addIcon(double x, double y, const QIcon& icon)
{
    return adoptAnnotation(new IconAnnotation(this, QPointF(x, y), icon));
}

RectAnnotation* PlotWidget::addRect(const QRectF& rect, const QString& label)
{
    auto* annotation = new RectAnnotation(this, rect);
    annotation->setLabel(label);
    return adoptAnnotation(annotation);
}

FlagPlacement PlotWidget::eventFlagPlacement() const noexcept
{
    return m_impl->flagPlacement;
}

void PlotWidget::setEventFlagPlacement(FlagPlacement placement)
{
    if (placement == m_impl->flagPlacement)
    {
        return;
    }
    m_impl->flagPlacement = placement;
    m_impl->headroomShare = 0.0;  // settled again as it is drawn
    refitY();
    invalidate();
    Q_EMIT eventFlagPlacementChanged();
}

Qt::Orientation PlotWidget::eventFlagOrientation() const noexcept
{
    return m_impl->flagOrientation;
}

void PlotWidget::setEventFlagOrientation(Qt::Orientation orientation)
{
    if (orientation == m_impl->flagOrientation)
    {
        return;
    }
    m_impl->flagOrientation = orientation;
    invalidate();
    Q_EMIT eventFlagOrientationChanged();
}

QList<Annotation*> PlotWidget::annotations() const
{
    return m_impl->annotations;
}

void PlotWidget::removeAnnotation(Annotation* annotation)
{
    if (!m_impl->annotations.removeOne(annotation))
    {
        return;
    }
    Q_EMIT annotationRemoved(annotation);
    // Nothing the pointer can be on may point at it until the plot is drawn again.
    std::erase_if(m_impl->annotationHits,
                  [annotation](const AnnotationHit& hit) { return hit.annotation == annotation; });
    if (m_impl->hovered == annotation)
    {
        m_impl->hovered = nullptr;
        Q_EMIT annotationHovered(nullptr);
    }
    delete annotation;
    applyAutoscale();
    invalidate();
}

void PlotWidget::clearAnnotations()
{
    while (!m_impl->annotations.isEmpty())
    {
        removeAnnotation(m_impl->annotations.back());
    }
}

Annotation* PlotWidget::annotationAt(QPointF position) const
{
    if (m_impl->legendLayout.box.contains(position))
    {
        return nullptr;
    }
    // The last drawn is on top.
    for (const AnnotationHit& hit : std::views::reverse(m_impl->annotationHits))
    {
        if (hit.box.contains(position))
        {
            const auto found = std::ranges::find(m_impl->annotations, hit.annotation);
            return found != m_impl->annotations.end() ? *found : nullptr;
        }
    }
    return nullptr;
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

ColorScale* PlotWidget::colorScale() const noexcept
{
    return m_impl->colorScale;
}

PlotLink* PlotWidget::link() const noexcept
{
    return m_impl->link;
}

bool PlotWidget::areYAxesZeroAligned() const noexcept
{
    return m_impl->zeroAligned;
}

void PlotWidget::setYAxesZeroAligned(bool aligned)
{
    if (aligned == m_impl->zeroAligned)
    {
        return;
    }
    m_impl->zeroAligned = aligned;
    refitY();
    invalidate();
    Q_EMIT yAxesZeroAlignedChanged();
}

double PlotWidget::aspectRatio() const noexcept
{
    return m_impl->aspectRatio;
}

void PlotWidget::setAspectRatio(double ratio)
{
    if (!std::isfinite(ratio) || ratio < 0.0)
    {
        qCWarning(lcData) << "PlotWidget::setAspectRatio: ignoring" << ratio;
        return;
    }
    if (ratio == m_impl->aspectRatio)
    {
        return;
    }
    m_impl->aspectRatio = ratio;
    if (ratio == 0.0)
    {
        // Back to what the axes were asked to show.
        const QScopedValueRollback settling(m_impl->settlingAspect, true);
        for (const auto& [axis, wanted] :
             {std::pair(m_impl->xAxis, m_impl->wantedX), std::pair(m_impl->yAxis, m_impl->wantedY)})
        {
            if (!axis->autoscale() && wanted)
            {
                axis->applyRange(*wanted);
            }
        }
    }
    applyAutoscale();  // which settles the aspect
    invalidate();
    Q_EMIT aspectRatioChanged();
}

void PlotWidget::setLink(PlotLink* link)
{
    m_impl->link    = link;
    m_impl->linkedX = std::nullopt;
    m_impl->history.clear();  // a view history now covers other plots, or no longer does
    invalidate();
}

void PlotWidget::setGrid(PlotGrid* grid)
{
    m_impl->grid = grid;
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
    if (PlotGrid* current = m_impl->grid.data(); current != nullptr)
    {
        current->realign();
    }
}

void PlotWidget::markDirty()
{
    m_impl->dirty      = true;
    m_impl->pointStale = true;
    m_impl->naturalMargins.reset();
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

QString PlotWidget::subtitle() const
{
    return m_impl->subtitle;
}

void PlotWidget::setSubtitle(const QString& subtitle)
{
    if (subtitle == m_impl->subtitle)
    {
        return;
    }
    m_impl->subtitle = subtitle;
    invalidate();
    Q_EMIT subtitleChanged();
}

QString PlotWidget::mathFontFamily() const
{
    return m_impl->text.mathFontFamily();
}

void PlotWidget::setMathFontFamily(const QString& family)
{
    if (family == m_impl->text.mathFontFamily())
    {
        return;
    }
    m_impl->text.setMathFontFamily(family);
    invalidate();
    Q_EMIT mathFontFamilyChanged();
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

void PlotWidget::zoom(double factor, Qt::Orientations orientations, std::optional<QPointF> center)
{
    if (!(factor > 0.0) || !std::isfinite(factor))
    {
        qCWarning(lcInput) << "PlotWidget::zoom: ignoring the factor" << factor;
        return;
    }
    if (hasFixedAspect())
    {
        orientations = Qt::Horizontal | Qt::Vertical;  // one zoom, or the ratio would change
    }
    const PlotLayout layout = m_impl->layout(*this);
    // The axes' new ranges, all from their ranges as they are now: a new x range refits the y
    // axes that autoscale.
    std::vector<std::pair<Axis*, Range>> zoomed;
    // Without a center, each axis zooms about its middle.
    const auto middle = [](const Axis& axis) {
        return core::middleOf(axis.range(), scaleOf(axis));
    };
    const auto zoomAxis = [factor, &zoomed](Axis& axis, double anchor) {
        const core::Scale scale = scaleOf(axis);
        const Range       range = axis.range();
        const Range       to    = core::zoomedAbout(range, anchor, factor, scale);
        if (to != range && core::isUsableRange(to, scale))
        {
            zoomed.emplace_back(&axis, to);
        }
    };
    Axis& x = *m_impl->xAxis;
    Axis& y = *m_impl->yAxis;
    if (orientations.testFlag(Qt::Horizontal))
    {
        zoomAxis(x, center ? center->x() : middle(x));
    }
    if (orientations.testFlag(Qt::Vertical))
    {
        zoomAxis(y, center ? center->y() : middle(y));
        if (Axis& y2 = *m_impl->yAxis2; layout.y2.shown)
        {
            // About the value at the same height on the right.
            zoomAxis(y2, center && layout.valid
                             ? layout.y2.mapping.toValue(layout.y.mapping.toPixel(center->y()))
                             : middle(y2));
        }
    }
    ViewHistory& views = history();
    views.beginStep();
    for (const auto& [axis, range] : zoomed)
    {
        axis->setRange(range);
    }
    settleAspect();
    views.commitStep();
}

void PlotWidget::back()
{
    history().back();
    settleAspect();
}

void PlotWidget::forward()
{
    history().forward();
    settleAspect();
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
    m_impl->crosshair  = enabled;
    m_impl->pointStale = true;
    if (!m_impl->plotCursor)
    {
        Q_EMIT plotCursorChanged();  // it is the crosshair's while it is on
    }
    if (isOutside(m_impl->legend->anchor()) && m_impl->legend->areValuesVisible())
    {
        invalidate();  // it keeps a column for the values, or no longer does
    }
    m_impl->linkedX.reset();
    if (enabled && underMouse())
    {
        setHoverPosition(mapFromGlobal(QPointF(QCursor::pos())));
    }
    update();
    Q_EMIT crosshairEnabledChanged();
}

CrosshairMode PlotWidget::crosshairMode() const noexcept
{
    return m_impl->crosshairMode;
}

void PlotWidget::setCrosshairMode(CrosshairMode mode)
{
    if (mode == m_impl->crosshairMode)
    {
        return;
    }
    m_impl->crosshairMode = mode;
    m_impl->pointStale    = true;
    update();
    Q_EMIT crosshairModeChanged();
    if (m_impl->crosshair && m_impl->hover)
    {
        syncCrosshair();  // it may be at another x now
        Q_EMIT crosshairMoved();
    }
}

const CrosshairPoint* PlotWidget::crosshairPoint() const
{
    Private& impl = *m_impl;
    if (!std::exchange(impl.pointStale, false))
    {
        return impl.point ? &*impl.point : nullptr;
    }
    impl.point.reset();
    const std::optional<QPointF> hover = impl.hover;
    if (!hover || !impl.crosshairFollowsData())
    {
        return nullptr;
    }
    const PlotLayout layout = impl.shownLayout(*this);
    if (!layout.valid)
    {
        return nullptr;
    }
    const QRectF&        area = layout.plot;
    const core::PixelBox box{
        .left   = area.left() - kEdgeTolerance,
        .top    = area.top() - kEdgeTolerance,
        .right  = area.right() + kEdgeTolerance,
        .bottom = area.bottom() + kEdgeTolerance,
    };
    std::optional<core::NearestPoint> nearest;
    Series*                           nearestSeries = nullptr;
    for (Series* series : std::as_const(impl.series))
    {
        if (!series->isVisible())
        {
            continue;
        }
        const core::PickQuery query{
            .x        = &layout.x.mapping,
            .y        = &layout.yFor(*series).mapping,
            .box      = box,
            .position = {.x = hover->x(), .y = hover->y()},
            .reach    = kSnapReach,
            .tracing  = impl.crosshairMode == CrosshairMode::TRACE,
        };
        core::NearestPoint found;
        // Of series equally near, the one drawn on top: the later.
        if (series->pick(query, found) && (!nearest || found.distance <= nearest->distance))
        {
            nearest       = found;
            nearestSeries = series;
        }
    }
    if (!nearest)
    {
        return nullptr;
    }
    impl.point = CrosshairPoint{
        .series   = nearestSeries,
        .index    = nearest->index,
        .position = QPointF(std::clamp(nearest->pixel.x, area.left(), area.right()),
                            std::clamp(nearest->pixel.y, area.top(), area.bottom())),
    };
    return &*impl.point;
}

Series* PlotWidget::crosshairSeries() const
{
    const CrosshairPoint* point = crosshairPoint();
    return point != nullptr ? point->series : nullptr;
}

std::optional<std::size_t> PlotWidget::crosshairIndex() const
{
    const CrosshairPoint* point = crosshairPoint();
    return point != nullptr ? std::optional(point->index) : std::nullopt;
}

std::optional<QPointF> PlotWidget::crosshairPosition() const
{
    if (!m_impl->crosshair)
    {
        return std::nullopt;
    }
    if (const CrosshairPoint* point = crosshairPoint())
    {
        // y is on yAxis(): for a point on the other y axis, what yAxis() reads at its height.
        const Series& series = *point->series;
        const double  y = series.isOnSecondaryYAxis()
                              ? m_impl->shownLayout(*this).y.mapping.toValue(point->position.y())
                              : series.y(point->index);
        return QPointF(series.x(point->index), y);
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
    m_impl->hover      = position;
    m_impl->pointStale = true;
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
    if (const CrosshairPoint* point = crosshairPoint())
    {
        x = point->series->x(point->index);
    }
    else if (const std::optional<QPointF> hover = m_impl->hover)
    {
        x = m_impl->shownLayout(*this).x.mapping.toValue(hover->x());
    }
    current->syncCrosshair(this, x);
}

Qt::CursorShape PlotWidget::plotCursor() const noexcept
{
    return m_impl->plotCursor.value_or(m_impl->crosshair ? Qt::CrossCursor : Qt::ArrowCursor);
}

void PlotWidget::setPlotCursor(Qt::CursorShape shape)
{
    if (m_impl->plotCursor == shape)
    {
        return;
    }
    m_impl->plotCursor = shape;
    updateCursor();
    Q_EMIT plotCursorChanged();
}

void PlotWidget::resetPlotCursor()
{
    if (!m_impl->plotCursor)
    {
        return;
    }
    m_impl->plotCursor.reset();
    updateCursor();
    Q_EMIT plotCursorChanged();
}

bool PlotWidget::wantsClicks() const
{
    return isSignalConnected(QMetaMethod::fromSignal(&PlotWidget::clicked));
}

void PlotWidget::emitClicked(QPointF widgetPosition, Qt::MouseButton button)
{
    Q_EMIT clicked(mapToData(widgetPosition), button);
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
        // Entries are clicked; the rest of the box is a handle, inside the plot area.
        if (m_impl->pointed)
        {
            setCursor(Qt::PointingHandCursor);
        }
        else if (isOutside(m_impl->legend->anchor()))
        {
            unsetCursor();
        }
        else
        {
            setCursor(Qt::OpenHandCursor);
        }
        return;
    }
    // Over the plot area, its own; elsewhere the widget's.
    const std::optional<QPointF> pointer = m_impl->pointer;
    const bool                   overPlot =
        m_impl->hover.has_value() ||
        (pointer.has_value() && m_impl->shownLayout(*this).plot.contains(*pointer));
    if (overPlot && (m_impl->plotCursor || m_impl->crosshair))
    {
        setCursor(plotCursor());
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
        showEntryMenu(*entry, globalPosition);
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
    // What the crosshair follows, as radio buttons. Choosing one also turns the crosshair on.
    auto*      modes   = new QActionGroup(menu);
    const auto addMode = [this, menu, modes](CrosshairMode mode, const QString& text) {
        QAction* action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(mode == crosshairMode());
        action->setActionGroup(modes);
        connect(action, &QAction::triggered, this, [this, mode] {
            setCrosshairMode(mode);
            setCrosshairEnabled(true);
        });
    };
    addMode(CrosshairMode::FREE, tr("Free"));
    addMode(CrosshairMode::SNAP, tr("Snap to data"));
    addMode(CrosshairMode::TRACE, tr("Trace data"));
    menu->addSeparator();
    menu->addAction(tr("Copy image"), this, [this] { copyToClipboard(); });
    menu->addAction(tr("Export…"), this, &PlotWidget::exportWithDialog);
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
    const auto shows = [this](const QObject* key) {
        const LegendEntry* entry = m_impl->legendLayout.entryOf(key);
        return entry != nullptr && entry->annotation == nullptr && entry->isVisible();
    };
    if (Annotation* under = position ? annotationAt(*position) : nullptr;
        under != m_impl->hovered.data())
    {
        m_impl->hovered = under;
        Q_EMIT annotationHovered(under);
    }
    m_impl->pointer             = position;
    const LegendEntry* entry    = position ? legendEntryAt(*position) : nullptr;
    QObject*           key      = entry != nullptr ? entry->key() : nullptr;
    const QObject*     previous = m_impl->pointed.data();
    if (key != previous)
    {
        // The highlighted series are drawn over the others, which fade.
        const bool redraw = shows(key) || shows(previous);
        m_impl->pointed   = key;
        if (redraw)
        {
            markDirty();
        }
        update();
    }
    updateCursor();
}

QList<const Series*> PlotWidget::highlightedSeries() const
{
    QList<const Series*> highlighted;
    const LegendEntry*   entry = m_impl->legendLayout.entryOf(m_impl->pointed.data());
    if (entry == nullptr || !m_impl->legend->isInteractive())
    {
        return highlighted;
    }
    for (const Series* series : entry->series)
    {
        if (series->isVisible())
        {
            highlighted.append(series);
        }
    }
    return highlighted;
}

void PlotWidget::toggleEntry(QObject* key)
{
    const LegendEntry* entry = m_impl->legendLayout.entryOf(key);
    if (entry == nullptr)
    {
        return;
    }
    if (entry->annotation != nullptr)
    {
        entry->annotation->setVisible(!entry->annotation->isVisible());
        return;
    }
    // Hidden all if any shows, else all shown.
    const bool                 visible = !entry->isVisible();
    const std::vector<Series*> members = entry->series;
    for (Series* series : members)
    {
        series->setVisible(visible);
    }
}

void PlotWidget::isolateEntry(QObject* key)
{
    const LegendEntry* entry = m_impl->legendLayout.entryOf(key);
    if (entry == nullptr || entry->annotation != nullptr)
    {
        return;
    }
    // Showing only the ones already shown alone shows them all again.
    const std::vector<Series*> members  = entry->series;
    const auto                 isMember = [&members](const Series* series) {
        return std::ranges::find(members, series) != members.end();
    };
    const bool alone =
        std::ranges::any_of(members, [](const Series* one) { return one->isVisible(); }) &&
        std::ranges::none_of(m_impl->series, [&](const Series* other) {
            return !isMember(other) && other->isVisible();
        });
    for (Series* series : std::as_const(m_impl->series))
    {
        series->setVisible(alone || isMember(series));
    }
}

void PlotWidget::showEntryMenu(const LegendEntry& entry, QPoint globalPosition)
{
    if (entry.annotation != nullptr)
    {
        showAnnotationEntryMenu(entry.annotation, globalPosition);
        return;
    }
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QList<QPointer<Series>> targets = pointersTo(entry.series);
    QAction*                      visible = menu->addAction(tr("Visible"));
    visible->setCheckable(true);
    visible->setChecked(entry.isVisible());
    connect(visible, &QAction::triggered, this,
            forEachOf(targets, [shown = !entry.isVisible()](Series& current) {
                current.setVisible(shown);
            }));
    menu->addAction(tr("Show only this"), this, [this, targets] {
        for (Series* other : std::as_const(m_impl->series))
        {
            other->setVisible(targets.contains(QPointer<Series>(other)));
        }
    });
    menu->addAction(tr("Show all"), this, [this] {
        for (Series* other : std::as_const(m_impl->series))
        {
            other->setVisible(true);
        }
    });
    menu->addSeparator();
    addStyleActions(menu, entry);
    menu->addSeparator();
    menu->addAction(tr("Remove"), this,
                    forEachOf(targets, [this](Series& current) { removeSeries(&current); }));
    Q_EMIT m_impl->legend->entryMenuAboutToShow(menu, entry.sample);
    menu->popup(globalPosition);
}

void PlotWidget::addStyleActions(QMenu* menu, const LegendEntry& entry)
{
    const QList<QPointer<Series>> targets = pointersTo(entry.series);
    const Series&                 sample  = *entry.sample;
    menu->addAction(tr("Color…"), this, [this, targets, color = sample.color()] {
        const QColor chosen = QColorDialog::getColor(color, this, tr("Series color"));
        if (chosen.isValid())
        {
            forEachOf(targets, [&chosen](Series& current) { current.setColor(chosen); })();
        }
    });
    if (std::ranges::any_of(entry.series,
                            [](const Series* one) { return one->m_color.has_value(); }))
    {
        menu->addAction(tr("Theme color"), this,
                        forEachOf(targets, [](Series& current) { current.resetColor(); }));
    }
    switch (sample.kind())
    {
        case SeriesKind::LINE:
            addLineWidthMenu(menu, entry);
            addMarkerMenu(menu, entry);
            break;
        case SeriesKind::SCATTER:
        case SeriesKind::BOX:  // its outliers'
            addMarkerMenu(menu, entry);
            break;
        case SeriesKind::BARS:
            break;  // neither a line nor markers
    }
}

void PlotWidget::addLineWidthMenu(QMenu* menu, const LegendEntry& entry)
{
    const QList<QPointer<Series>> targets = pointersTo(entry.series);
    const auto*                   line    = qobject_cast<const LineSeries*>(entry.sample);
    if (line == nullptr)
    {
        return;
    }
    QMenu*        widths = menu->addMenu(tr("Line width"));
    const QLocale locale;
    for (const double width : {1.0, 1.5, 2.0, 3.0, 4.0})
    {
        QAction* action = widths->addAction(tr("%1 px").arg(locale.toString(width)));
        action->setCheckable(true);
        action->setChecked(line->lineWidth() == width);
        // A group may hold series of other kinds, which have no line width.
        connect(action, &QAction::triggered, this, forEachOf(targets, [width](Series& current) {
                    if (auto* currentLine = qobject_cast<LineSeries*>(&current))
                    {
                        currentLine->setLineWidth(width);
                    }
                }));
    }
}

void PlotWidget::addMarkerMenu(QMenu* menu, const LegendEntry& entry)
{
    const QList<QPointer<Series>> targets   = pointersTo(entry.series);
    const Series&                 sample    = *entry.sample;
    QMenu*                        markers   = menu->addMenu(tr("Marker"));
    const auto                    shapes    = std::to_array<std::pair<Marker, QString>>({
        {Marker::NONE, tr("None")},
        {Marker::CIRCLE, tr("Circle")},
        {Marker::SQUARE, tr("Square")},
        {Marker::DIAMOND, tr("Diamond")},
        {Marker::TRIANGLE, tr("Triangle")},
        {Marker::CROSS, tr("Cross")},
        {Marker::PLUS, tr("Plus")},
    });
    bool                          noneShows = true;  // whether a series without markers draws
    switch (sample.kind())
    {
        case SeriesKind::LINE:
        case SeriesKind::BOX:
            break;
        case SeriesKind::SCATTER:
        case SeriesKind::BARS:
            noneShows = false;
            break;
    }
    for (const auto& [shape, name] : shapes)
    {
        if (shape == Marker::NONE && !noneShows)
        {
            continue;
        }
        QAction* action = markers->addAction(markerIcon(sample, shape), name);
        action->setCheckable(true);
        action->setChecked(sample.marker() == shape);
        connect(action, &QAction::triggered, this,
                forEachOf(targets, [shape](Series& current) { current.setMarker(shape); }));
    }
}

void PlotWidget::showAnnotationEntryMenu(Annotation* annotation, QPoint globalPosition)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QPointer<Annotation> target(annotation);
    const auto                 onTarget = [target](auto&& action) {
        return [target, action] {
            if (Annotation* current = target.data())
            {
                action(*current);
            }
        };
    };
    QAction* visible = menu->addAction(tr("Visible"));
    visible->setCheckable(true);
    visible->setChecked(annotation->isVisible());
    connect(visible, &QAction::triggered, this,
            onTarget([](Annotation& current) { current.setVisible(!current.isVisible()); }));
    menu->addSeparator();
    menu->addAction(tr("Color…"), this, onTarget([this](Annotation& current) {
                        const QColor color =
                            QColorDialog::getColor(current.color(), this, tr("Annotation color"));
                        if (color.isValid())
                        {
                            current.setColor(color);
                        }
                    }));
    if (annotation->m_color)
    {
        menu->addAction(tr("Theme color"), this,
                        onTarget([](Annotation& current) { current.resetColor(); }));
    }
    menu->addSeparator();
    menu->addAction(tr("Remove"), this,
                    onTarget([this](Annotation& current) { removeAnnotation(&current); }));
    Q_EMIT m_impl->legend->annotationEntryMenuAboutToShow(menu, annotation);
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
    refitColorScale();
    settleAspect();
}

void PlotWidget::refitColorScale()
{
    ColorScale& scale = *m_impl->colorScale;
    if (!scale.autoscale())
    {
        return;
    }
    Range bounds = Range::empty();
    for (const Series* series : std::as_const(m_impl->series))
    {
        const auto* scatter = qobject_cast<const ScatterSeries*>(series);
        if (scatter != nullptr && scatter->isVisible())
        {
            bounds = bounds.united(scatter->colorBounds());
        }
    }
    if (!bounds.isValid())
    {
        return;  // nothing to fit: stays where it is
    }
    if (!core::isUsableRange(bounds))
    {
        // One value: a range around it, as an axis has.
        bounds = core::autoscaleRange(bounds, 0.0);
    }
    scale.applyRange(bounds);
}

Range PlotWidget::xDataBounds(bool positiveOnly, bool withAnnotations) const
{
    Range bounds = Range::empty();
    for (const Series* series : std::as_const(m_impl->series))
    {
        if (series->isVisible())
        {
            bounds = bounds.united(series->fitBoundsX(positiveOnly));
        }
    }
    if (!withAnnotations)
    {
        return bounds;
    }
    for (const Annotation* annotation : std::as_const(m_impl->annotations))
    {
        if (annotation->isVisible() && annotation->isIncludedInAutoscale() &&
            annotation->coordinates() == AnnotationCoordinates::DATA)
        {
            const Range extent = annotation->xExtent();
            bounds             = bounds.united(positiveOnly ? positivePart(extent) : extent);
        }
    }
    return bounds;
}

Range PlotWidget::yDataBounds(const Axis& axis, bool positiveOnly, bool visibleOnly) const
{
    const Range xRange = m_impl->xAxis->range();
    Range       bounds = Range::empty();
    for (const Series* series : std::as_const(m_impl->series))
    {
        if (series->isVisible() && series->isOnSecondaryYAxis() == axis.isSecondary())
        {
            bounds = bounds.united(visibleOnly ? series->fitBoundsYWithin(xRange, positiveOnly)
                                               : series->fitBoundsY(positiveOnly));
        }
    }
    for (const Annotation* annotation : std::as_const(m_impl->annotations))
    {
        if (annotation->isVisible() && annotation->isIncludedInAutoscale() &&
            annotation->coordinates() == AnnotationCoordinates::DATA &&
            annotation->isOnSecondaryYAxis() == axis.isSecondary())
        {
            const Range extent = annotation->yExtent();
            bounds             = bounds.united(positiveOnly ? positivePart(extent) : extent);
        }
    }
    return bounds;
}

void PlotWidget::refitX()
{
    if (const std::optional<Range> fit = fitX())
    {
        m_impl->xAxis->applyRange(*fit);
    }
}

std::optional<Range> PlotWidget::fitX() const
{
    const Axis& axis = *m_impl->xAxis;
    if (!axis.autoscale())
    {
        return std::nullopt;
    }
    const core::Scale scale = scaleOf(axis);
    const bool        log   = scale == core::Scale::LOG;
    // Following the newest data means the data: an annotation further on isn't the newest of it.
    const bool      follow = axis.autoscaleMode() == AutoscaleMode::FOLLOW_LATEST && !log;
    const PlotLink* link   = m_impl->link.data();
    const Range     bounds =
        link != nullptr ? link->xDataBounds(log, !follow) : xDataBounds(log, !follow);
    if (axis.scaleType() == ScaleType::CATEGORY)
    {
        return core::categoryRange(bounds, static_cast<std::size_t>(axis.categories().size()));
    }
    if (follow)
    {
        return core::followRange(bounds, axis.followWindow(), axis.autoscaleMargin());
    }
    return core::autoscaleRange(bounds, axis.autoscaleMargin(), scale,
                                {
                                    .includeWithin = axis.autoscaleIncludesZero(),
                                    .baseline      = standsOnZero(Qt::Horizontal, nullptr),
                                });
}

std::optional<Range> PlotWidget::fitY(const Axis& axis) const
{
    if (!axis.autoscale())
    {
        return std::nullopt;
    }
    const core::Scale scale = scaleOf(axis);
    // With a fixed aspect, y's range decides x's, which mustn't decide y's in turn.
    const bool  visible = axis.autoscaleMode() != AutoscaleMode::FIT_ALL &&
                          (&axis != m_impl->yAxis || !hasFixedAspect());
    const Range bounds  = yDataBounds(axis, scale == core::Scale::LOG, visible);
    if (axis.scaleType() == ScaleType::CATEGORY)
    {
        return core::categoryRange(bounds, static_cast<std::size_t>(axis.categories().size()));
    }
    // Fitting what's visible: with nothing in view, stay put rather than jump to a default.
    if (visible && !bounds.isValid())
    {
        return std::nullopt;
    }
    return core::autoscaleRange(bounds, axis.autoscaleMargin(), scale,
                                {
                                    .includeWithin = axis.autoscaleIncludesZero(),
                                    .baseline      = standsOnZero(Qt::Vertical, &axis),
                                });
}

bool PlotWidget::standsOnZero(Qt::Orientation orientation, const Axis* yAxis) const
{
    return std::ranges::any_of(m_impl->series, [&](const Series* series) {
        return series->isVisible() && series->zeroBaselines().testFlag(orientation) &&
               (yAxis == nullptr || series->yAxis() == yAxis);
    });
}

void PlotWidget::refitY()
{
    const RangeOverrides fitted = headroomRanges(m_impl->headroomShare);
    if (fitted.y)
    {
        m_impl->yAxis->applyRange(*fitted.y);
    }
    if (fitted.y2)
    {
        m_impl->yAxis2->applyRange(*fitted.y2);
    }
}

RangeOverrides PlotWidget::headroomRanges(double share) const
{
    // Fit, line the zeros up, then leave the headroom: the same share of both keeps them in line.
    std::optional<Range> y  = fitY(*m_impl->yAxis);
    std::optional<Range> y2 = fitY(*m_impl->yAxis2);
    if (m_impl->zeroAligned && y && y2 && m_impl->yAxis->scaleType() == ScaleType::LINEAR &&
        m_impl->yAxis2->scaleType() == ScaleType::LINEAR && m_impl->yAxis->isShown(true) &&
        m_impl->yAxis2->isShown(hasSeriesOnSecondary(*this)))
    {
        const core::AlignedZeros aligned = core::alignZeros(*y, *y2);
        y                                = aligned.first;
        y2                               = aligned.second;
    }
    if (m_impl->flagPlacement == FlagPlacement::HEADROOM)
    {
        const auto withHeadroom = [&](std::optional<Range>& range, const Axis& axis) {
            if (range)
            {
                range = core::withHeadroom(*range, share, scaleOf(axis));
            }
        };
        withHeadroom(y, *m_impl->yAxis);
        withHeadroom(y2, *m_impl->yAxis2);
    }
    return {.x = std::nullopt, .y = y, .y2 = y2};
}

double PlotWidget::headroomShare(const QRectF& bounds, double devicePixelRatio,
                                 const LayoutConstraints& constraints) const
{
    if (m_impl->flagPlacement != FlagPlacement::HEADROOM)
    {
        return 0.0;
    }
    // The room the rows of flags in use take, as a share of the plot area's height: the rows
    // depend a little on the ranges (through the y labels' width), which depend on the share. It
    // only grows from one layout to the next, so it settles.
    double share = 0.0;
    for (int pass = 0; pass < kHeadroomPasses; ++pass)
    {
        const PlotLayout layout = layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text,
                                             constraints, headroomRanges(share));
        if (!layout.valid || layout.flags.flags.empty())
        {
            break;
        }
        const double needed =
            std::min((kFlagTop + layout.flags.height + kHeadroomGap) / layout.plot.height(),
                     kMaxHeadroomShare);
        if (needed <= share + kHeadroomEpsilon)
        {
            break;
        }
        share = needed;
    }
    return share;
}

void PlotWidget::settleHeadroom()
{
    const double share =
        headroomShare(QRectF(rect()), devicePixelRatioF(), m_impl->constraints(*this));
    if (std::abs(share - m_impl->headroomShare) > kHeadroomEpsilon)
    {
        m_impl->headroomShare = share;
        refitY();
    }
}

bool PlotWidget::hasFixedAspect() const
{
    return m_impl->aspectRatio > 0.0 && m_impl->xAxis->scaleType() == ScaleType::LINEAR &&
           m_impl->yAxis->scaleType() == ScaleType::LINEAR;
}

RangeOverrides PlotWidget::aspectRanges(const QRectF& bounds, double devicePixelRatio,
                                        LayoutConstraints&    constraints,
                                        const RangeOverrides& fitted) const
{
    const Axis& x = *m_impl->xAxis;
    const Axis& y = *m_impl->yAxis;
    // What each axis is asked to show: what it fits, or the range it was given (a linked x axis:
    // the link's).
    const bool  linked = m_impl->link != nullptr;
    const Range wantedX =
        linked ? x.range() : (x.autoscale() ? fitX() : m_impl->wantedX).value_or(x.range());
    const Range     wantedY = (y.autoscale() ? fitted.y : m_impl->wantedY).value_or(y.range());
    core::AspectFit fit     = core::AspectFit::WIDEN;  // both given alike
    if (linked || (x.autoscale() && x.autoscaleMode() == AutoscaleMode::FOLLOW_LATEST) ||
        (!x.autoscale() && y.autoscale()))
    {
        fit = core::AspectFit::FOLLOW_X;
    }
    else if (x.autoscale() && !y.autoscale())
    {
        fit = core::AspectFit::FOLLOW_Y;
    }
    RangeOverrides ranges = fitted;
    ranges.x              = wantedX;
    ranges.y              = wantedY;
    // Ranges already to scale in the plot area the margins kept from the last settling leave
    // (zoomed or panned, or a box fitted to that plot area) stay, with those margins.
    const LayoutConstraints kept = widest(constraints, m_impl->aspectMargins);
    if (const PlotLayout layout =
            layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text, kept, ranges);
        layout.valid && isToScale(wantedX, wantedY, layout.plot, m_impl->aspectRatio))
    {
        constraints = kept;
        return ranges;
    }
    // Otherwise: the plot area depends a little on the ranges (through the y labels' width), so
    // lay out until it holds. Two ranges can each have the labels that fit the other in
    // (narrower ones, a wider plot area, ranges with wider labels, and back): once the ranges
    // are settled the plot area keeps the widest margins they took.
    for (int round = 0; round < kAspectRounds; ++round)
    {
        const PlotLayout layout =
            layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text, constraints, ranges);
        if (!layout.valid)
        {
            break;
        }
        const core::AspectRanges aspect = core::fitAspect(
            wantedX, wantedY, layout.plot.width(), layout.plot.height(), m_impl->aspectRatio, fit);
        if (aspect.x == ranges.x && aspect.y == ranges.y)
        {
            break;
        }
        if (round > 0)
        {
            constraints = widest(constraints, marginsOf(bounds, layout.plot));
        }
        ranges.x = aspect.x;
        ranges.y = aspect.y;
    }
    return ranges;
}

void PlotWidget::settleAspect() const
{
    if (!hasFixedAspect() || m_impl->settlingAspect)
    {
        return;
    }
    LayoutConstraints    margins = m_impl->alignedConstraints(*this);
    const RangeOverrides ranges  = aspectRanges(QRectF(rect()), devicePixelRatioF(), margins,
                                                headroomRanges(m_impl->headroomShare));
    if (margins != m_impl->aspectMargins)
    {
        m_impl->aspectMargins = margins;
        m_impl->dirty         = true;
    }
    const Range                x = ranges.x.value_or(m_impl->xAxis->range());
    const Range                y = ranges.y.value_or(m_impl->yAxis->range());
    const QScopedValueRollback settling(m_impl->settlingAspect, true);
    // x first: a new x range refits the y axes.
    m_impl->xAxis->applyRange(x);
    m_impl->yAxis->applyRange(y);
}

void PlotWidget::showWithAspect(Range x, Range y, Qt::Orientations given)
{
    core::AspectFit fit = core::AspectFit::WIDEN;
    if (given == Qt::Horizontal)
    {
        fit = core::AspectFit::FOLLOW_X;
    }
    else if (given == Qt::Vertical)
    {
        fit = core::AspectFit::FOLLOW_Y;
    }
    const PlotLayout         layout = m_impl->layout(*this);
    const core::AspectRanges shown =
        layout.valid ? core::fitAspect(x, y, layout.plot.width(), layout.plot.height(),
                                       m_impl->aspectRatio, fit)
                     : core::AspectRanges{.x = x, .y = y};
    m_impl->xAxis->setRange(shown.x);
    m_impl->yAxis->setRange(shown.y);
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
        case ThemeMode::HIGH_CONTRAST:
            m_impl->theme = Theme::highContrast();
            break;
        case ThemeMode::PRINT:
            m_impl->theme = Theme::print();
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

// Output
// --------------------------------------------------------------------------------------------------------

void PlotWidget::paintExport(QPainter& painter, const QRectF& bounds, double devicePixelRatio) const
{
    // The margins the screen keeps to the aspect ratio only if they suit this shape too.
    LayoutConstraints    constraints = m_impl->alignedConstraints(*this);
    const RangeOverrides ranges      = exportRanges(bounds, devicePixelRatio, constraints);
    paintLayout(painter, layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text,
                                    constraints, ranges));
}

void PlotWidget::paintExport(QPainter& painter, const QRectF& bounds, double devicePixelRatio,
                             const LayoutConstraints&    constraints,
                             const std::optional<Theme>& theme) const
{
    const ThemeOverride  used(m_impl->theme, theme);
    LayoutConstraints    kept   = constraints;
    const RangeOverrides ranges = exportRanges(bounds, devicePixelRatio, kept);
    paintLayout(painter,
                layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text, kept, ranges));
}

RangeOverrides PlotWidget::exportRanges(const QRectF& bounds, double devicePixelRatio,
                                        LayoutConstraints& constraints) const
{
    // The flags take as many pixels in an export as on screen, and so another share of a plot
    // area of another height; and a plot area of another shape widens another axis for the
    // aspect ratio.
    const bool headroom = m_impl->flagPlacement == FlagPlacement::HEADROOM;
    if (!headroom && !hasFixedAspect())
    {
        return {};
    }
    const RangeOverrides fitted =
        headroomRanges(headroom ? headroomShare(bounds, devicePixelRatio, constraints) : 0.0);
    return hasFixedAspect() ? aspectRanges(bounds, devicePixelRatio, constraints, fitted) : fitted;
}

void PlotWidget::paintLayout(QPainter& painter, const PlotLayout& layout) const
{
    core::Occupancy occupancy;
    RenderStats     stats;
    const bool      best = m_impl->legend->anchor() == LegendAnchor::BEST;
    PlotRenderer    renderer(*this, layout, m_impl->markers, m_impl->text);
    renderer.render(
        painter, stats,
        {.highlighted = {}, .occupancy = best ? &occupancy : nullptr, .annotationHits = nullptr});
    // The legend where it hides the least in this layout, staying where it is on screen if that is
    // about as good.
    const LegendLayout legend = layoutLegend(*this, layout, m_impl->text,
                                             {
                                                 .crosshairX    = std::nullopt,
                                                 .minValueWidth = 0.0,
                                                 .occupancy     = &occupancy,
                                                 .previousBest  = m_impl->bestAnchor,
                                             });
    drawLegend(painter, *this, layout, legend, m_impl->text, nullptr);
}

QSize PlotWidget::exportSize(const ExportOptions& options) const
{
    return options.size.isEmpty() ? size() : options.size;
}

void PlotWidget::paint(QPainter& painter, const QRectF& rect) const
{
    const QPaintDevice* device = painter.device();
    painter.save();
    paintExport(painter, rect, device != nullptr ? device->devicePixelRatioF() : 1.0);
    painter.restore();
}

QImage PlotWidget::renderToImage(const ExportOptions& options) const
{
    const ThemeOverride theme(m_impl->theme, options.theme);
    const double pixelRatio = options.dpi > 0.0 ? options.dpi / kBaseDpi : devicePixelRatioF();
    return paintImage(exportSize(options), pixelRatio,
                      [this](QPainter& painter, const QRectF& bounds, double ratio) {
                          paintExport(painter, bounds, ratio);
                      });
}

bool PlotWidget::exportImage(const QString& fileName, const ExportOptions& options) const
{
    const QImage image = renderToImage(options);
    if (image.isNull() || !image.save(fileName))
    {
        qCWarning(lcRender) << "PlotWidget::exportImage: cannot write" << fileName;
        return false;
    }
    return true;
}

bool PlotWidget::exportSvg(const QString& fileName, const ExportOptions& options) const
{
    const ThemeOverride theme(m_impl->theme, options.theme);
    const bool          written =
        writeSvg(fileName, exportSize(options), logicalDpiY(), plainText(m_impl->title),
                 [this](QPainter& painter, const QRectF& bounds, double ratio) {
                     paintExport(painter, bounds, ratio);
                 });
    if (!written)
    {
        qCWarning(lcRender) << "PlotWidget::exportSvg: cannot write" << fileName;
    }
    return written;
}

bool PlotWidget::exportPdf(const QString& fileName, const ExportOptions& options) const
{
    const ThemeOverride theme(m_impl->theme, options.theme);
    const bool          written =
        writePdf(fileName, exportSize(options), logicalDpiY(), plainText(m_impl->title),
                 options.creator, [this](QPainter& painter, const QRectF& bounds, double ratio) {
                     paintExport(painter, bounds, ratio);
                 });
    if (!written)
    {
        qCWarning(lcRender) << "PlotWidget::exportPdf: cannot write" << fileName;
    }
    return written;
}

bool PlotWidget::exportTo(const QString& fileName, const ExportOptions& options) const
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("svg"))
    {
        return exportSvg(fileName, options);
    }
    if (suffix == QLatin1String("pdf"))
    {
        return exportPdf(fileName, options);
    }
    if (suffix == QLatin1String("csv"))
    {
        return exportCsv(fileName);
    }
    if (suffix == QLatin1String("tsv"))
    {
        return exportCsv(fileName, QLatin1Char('\t'));
    }
    return exportImage(fileName, options);
}

void PlotWidget::copyToClipboard(const ExportOptions& options) const
{
    const QImage image = renderToImage(options);
    if (!image.isNull())
    {
        QGuiApplication::clipboard()->setImage(image);
    }
}

void PlotWidget::writeCsv(QChar separator, const std::function<void(std::string_view)>& write) const
{
    std::vector<core::CsvSeries> table;
    qsizetype                    number = 0;
    for (const Series* series : std::as_const(m_impl->series))
    {
        ++number;
        if (!series->isVisible())
        {
            continue;
        }
        const QString name =
            series->name().isEmpty() ? tr("Series %1").arg(number) : plainText(series->name());
        core::CsvSeries& columns = table.emplace_back(core::CsvSeries{
            .name   = name.toStdString(),
            .data   = &series->data(),
            .errors = &series->errors(),
        });
        series->addCsvColumns(columns);
    }
    const QString label = plainText(m_impl->xAxis->label());
    core::writeCsv(
        {
            .series    = table,
            .xRange    = m_impl->xAxis->range(),
            .xName     = label.isEmpty() ? std::string("x") : label.toStdString(),
            .separator = separator.toLatin1() != 0 ? separator.toLatin1() : ',',
        },
        write);
}

QString PlotWidget::toCsv(QChar separator) const
{
    std::string text;
    writeCsv(separator, [&text](std::string_view piece) { text += piece; });
    return QString::fromStdString(text);
}

bool PlotWidget::exportCsv(const QString& fileName, QChar separator) const
{
    // Complete or not at all: a failed export leaves no half-written file.
    QSaveFile file(fileName);
    bool      written = file.open(QIODevice::WriteOnly);
    if (written)
    {
        writeCsv(separator, [&](std::string_view piece) {
            written = written && file.write(piece.data(), static_cast<qint64>(piece.size())) ==
                                     static_cast<qint64>(piece.size());
        });
        written = written && file.commit();
    }
    if (!written)
    {
        qCWarning(lcData) << "PlotWidget::exportCsv: cannot write" << fileName;
    }
    return written;
}

void PlotWidget::exportWithDialog()
{
    QString filter;
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Export plot"), QString(),
        tr("PNG image (*.png);;JPEG image (*.jpg);;SVG drawing (*.svg);;PDF document (*.pdf);;"
           "CSV data (*.csv)"),
        &filter);
    if (fileName.isEmpty())
    {
        return;
    }
    if (QFileInfo(fileName).suffix().isEmpty())
    {
        // A name typed without a suffix gets the one of the kind of file chosen: "(*.png)".
        const qsizetype dot = filter.lastIndexOf(QLatin1Char('.'));
        fileName += filter.mid(dot).chopped(1);
    }
    if (!exportTo(fileName))
    {
        QMessageBox::warning(this, tr("Export plot"),
                             tr("Could not write %1.").arg(QDir::toNativeSeparators(fileName)));
    }
}

// State
// ---------------------------------------------------------------------------------------------------------

QJsonObject PlotWidget::saveState() const
{
    QJsonObject state;
    state.insert(QLatin1String("format"), QString(state::kFormat));
    state.insert(QLatin1String("version"), state::kVersion);
    state.insert(QLatin1String("themeMode"), state::fromEnum(m_impl->themeMode));
    state.insert(QLatin1String("crosshair"), m_impl->crosshair);
    state.insert(QLatin1String("crosshairMode"), state::fromEnum(m_impl->crosshairMode));
    state.insert(QLatin1String("yAxesZeroAligned"), m_impl->zeroAligned);
    state.insert(QLatin1String("aspectRatio"), m_impl->aspectRatio);
    state.insert(QLatin1String("eventFlagPlacement"), state::fromEnum(m_impl->flagPlacement));
    state.insert(QLatin1String("eventFlagOrientation"), state::fromEnum(m_impl->flagOrientation));
    for (const auto& [key, axis] : m_impl->axesByKey())
    {
        QJsonObject settings;
        axis->saveState(settings);
        state.insert(key, settings);
    }
    QJsonObject legend;
    m_impl->legend->saveState(legend);
    state.insert(QLatin1String("legend"), legend);
    QJsonObject colorScale;
    m_impl->colorScale->saveState(colorScale);
    state.insert(QLatin1String("colorScale"), colorScale);
    QJsonArray series;
    for (const Series* one : std::as_const(m_impl->series))
    {
        QJsonObject settings;
        one->saveState(settings);
        series.append(settings);
    }
    state.insert(QLatin1String("series"), series);
    return state;
}

bool PlotWidget::restoreState(const QJsonObject& state)
{
    const int version = state.value(QLatin1String("version")).toInt();
    if (state.value(QLatin1String("format")).toString() != state::kFormat || version < 1 ||
        version > state::kVersion)
    {
        qCWarning(lcData) << "PlotWidget::restoreState: not a state this version reads";
        return false;
    }
    // A custom theme's colors aren't saved: a plot that had one keeps the theme it has now.
    if (const auto mode = state::toEnum<ThemeMode>(state.value(QLatin1String("themeMode")));
        mode && *mode != ThemeMode::CUSTOM)
    {
        setThemeMode(*mode);
    }
    if (const auto crosshair = state::toBool(state.value(QLatin1String("crosshair"))))
    {
        setCrosshairEnabled(*crosshair);
    }
    if (const auto mode = state::toEnum<CrosshairMode>(state.value(QLatin1String("crosshairMode"))))
    {
        setCrosshairMode(*mode);
    }
    if (const auto aligned = state::toBool(state.value(QLatin1String("yAxesZeroAligned"))))
    {
        setYAxesZeroAligned(*aligned);
    }
    if (const auto ratio = state::toNumber(state.value(QLatin1String("aspectRatio"))))
    {
        setAspectRatio(*ratio);
    }
    if (const auto placement =
            state::toEnum<FlagPlacement>(state.value(QLatin1String("eventFlagPlacement"))))
    {
        setEventFlagPlacement(*placement);
    }
    if (const auto orientation =
            state::toEnum<Qt::Orientation>(state.value(QLatin1String("eventFlagOrientation"))))
    {
        setEventFlagOrientation(*orientation);
    }
    // The series before the axes: which of them are shown, and against which y axis, decides
    // what an autoscaling axis fits.
    QList<Series*>   unmatched = m_impl->series;
    const QJsonArray series    = state.value(QLatin1String("series")).toArray();
    for (const auto& entry : series)
    {
        const QJsonObject settings = entry.toObject();
        const QString     name     = settings.value(QLatin1String("name")).toString();
        const auto        match    = std::ranges::find_if(
            unmatched, [&name](const Series* one) { return one->name() == name; });
        if (match != unmatched.end())
        {
            (*match)->restoreState(settings);
            unmatched.erase(match);
        }
    }
    for (const auto& [key, axis] : m_impl->axesByKey())
    {
        if (const QJsonValue settings = state.value(key); settings.isObject())
        {
            axis->restoreState(settings.toObject());
        }
    }
    if (const QJsonValue legend = state.value(QLatin1String("legend")); legend.isObject())
    {
        m_impl->legend->restoreState(legend.toObject());
    }
    if (const QJsonValue scale = state.value(QLatin1String("colorScale")); scale.isObject())
    {
        m_impl->colorScale->restoreState(scale.toObject());
    }
    invalidate();
    return true;
}

// Geometry
// ------------------------------------------------------------------------------------------------------

QRectF PlotWidget::plotArea() const
{
    settleAspect();
    return m_impl->layout(*this).plot;
}

QRectF PlotWidget::legendArea() const
{
    return m_impl->legendLayout.box;
}

QPointF PlotWidget::mapToData(QPointF widgetPosition, const Axis* yAxis) const
{
    settleAspect();
    const PlotLayout  layout = m_impl->layout(*this);
    const AxisLayout& y      = yAxis == m_impl->yAxis2 ? layout.y2 : layout.y;
    return {layout.x.mapping.toValue(widgetPosition.x()), y.mapping.toValue(widgetPosition.y())};
}

QPointF PlotWidget::mapFromData(QPointF dataPosition, const Axis* yAxis) const
{
    settleAspect();
    const PlotLayout  layout = m_impl->layout(*this);
    const AxisLayout& y      = yAxis == m_impl->yAxis2 ? layout.y2 : layout.y;
    return {layout.x.mapping.toPixel(dataPosition.x()), y.mapping.toPixel(dataPosition.y())};
}

QMarginsF PlotWidget::naturalMargins() const
{
    const std::optional<Private::NaturalMargins>& kept = m_impl->naturalMargins;
    if (!kept || kept->size != size() || kept->devicePixelRatio != devicePixelRatioF())
    {
        m_impl->naturalMargins = Private::NaturalMargins{
            .margins          = naturalMargins(QRectF(rect()), devicePixelRatioF(), std::nullopt),
            .size             = size(),
            .devicePixelRatio = devicePixelRatioF(),
        };
    }
    return m_impl->naturalMargins->margins;
}

QMarginsF PlotWidget::naturalMargins(const QRectF& bounds, double devicePixelRatio,
                                     const std::optional<Theme>& theme) const
{
    const ThemeOverride used(m_impl->theme, theme);
    const PlotLayout    layout =
        layoutPlot(*this, bounds, font(), devicePixelRatio, m_impl->text, LayoutConstraints{});
    return {layout.naturalLeft, layout.naturalTop, layout.naturalRight, layout.naturalBottom};
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
        case QEvent::ToolTip:
            // An annotation's tool tip, as long as the pointer stays on what it drew; else the
            // widget's own, if it has one.
            if (const auto* help = dynamic_cast<QHelpEvent*>(event); help != nullptr)
            {
                const QPointF position(help->pos());
                const auto hit = std::ranges::find_if(std::views::reverse(m_impl->annotationHits),
                                                      [&position](const AnnotationHit& candidate) {
                                                          return candidate.box.contains(position);
                                                      });
                if (const Annotation* annotation = annotationAt(position);
                    annotation != nullptr && !annotation->toolTip().isEmpty())
                {
                    QToolTip::showText(help->globalPos(), annotation->toolTip(), this,
                                       hit->box.toAlignedRect());
                    return true;
                }
            }
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
    settleHeadroom();
    settleAspect();
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
        m_impl->annotationHits.clear();
        renderer.render(painter, stats,
                        {
                            .highlighted    = highlightedSeries(),
                            .occupancy      = best ? &m_impl->occupancy : nullptr,
                            .annotationHits = &m_impl->annotationHits,
                        });
    }
    stats.milliseconds = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    m_impl->stats      = std::move(stats);
    m_impl->dirty      = false;
    m_impl->pointStale = true;  // the crosshair is on a point of the plot as now drawn
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
        drawCrosshair(painter, *this, layout, m_impl->hover, m_impl->linkedX, crosshairPoint());
    }

    // The legend, with the values at the crosshair.
    const std::optional<QPointF> crosshair = crosshairPosition();
    const CrosshairPoint*        onPoint   = crosshairPoint();
    if (!crosshair)
    {
        m_impl->valueWidth = 0.0;
    }
    m_impl->legendLayout =
        layoutLegend(*this, layout, m_impl->text,
                     {
                         .crosshairX = crosshair ? std::optional(crosshair->x()) : std::nullopt,
                         .crosshairSeries = onPoint != nullptr ? onPoint->series : nullptr,
                         .crosshairIndex  = onPoint != nullptr ? onPoint->index : 0,
                         .minValueWidth   = m_impl->valueWidth,
                         .occupancy       = &m_impl->occupancy,
                         .previousBest    = m_impl->bestAnchor,
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
    settleAspect();
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
    // Not QWidget's, which would take it for a press: the controller has had it as one.
    event->ignore();
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
