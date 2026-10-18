#include "PlotLayout.h"

#include <QDateTime>
#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "FlagLayout.h"
#include "LegendLayout.h"
#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/AxisTicks.h"
#include "core/NumberFormatter.h"
#include "core/TimeTicks.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/ColorScale.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"
#include "rocketplot/plottime.h"

namespace rocketplot
{

namespace
{

constexpr double kOuterPadding  = 10.0;
constexpr double kTitleGap      = 10.0;  // under the title (and subtitle)
constexpr double kSubtitleGap   = 2.0;   // between the title and the subtitle
constexpr double kAxisLabelGap  = 6.0;
constexpr double kAnnotationGap = 4.0;
constexpr double kMinPlotSize   = 16.0;
constexpr double kMinorSpacing  = 4.0;
// Space between neighboring x tick labels, and the height y tick labels need per tick (in label
// heights): numbers well apart, names (of categories) in rows.
constexpr double kXLabelSpacing         = 16.0;
constexpr double kYLabelSpacing         = 2.2;
constexpr double kYCategoryLabelSpacing = 1.2;
// Rounds of x tick layout: labels may turn out wider than guessed, or overflow the right edge.
constexpr int kXLayoutAttempts = 4;
// Rough share of the target the plot area gets, for the first guess at which axes need an
// annotation row.
constexpr double kProbeShare = 0.7;
// The x label spacing assumed by that guess.
constexpr double kProbeSpacing = 80.0;
// Event flags: the share of the plot area's height their rows may take over the data, and of the
// whole height a band above the plot area may take; the gap between the band and the plot area;
// and how many layouts a band's height may take to settle.
constexpr double kFlagRowShare = 0.5;
constexpr double kMaxBandShare = 0.3;
constexpr double kBandGap      = 4.0;
constexpr int    kBandPasses   = 3;
// A legend outside the plot area: the gap between it and what is next to it, and the share of the
// height (under the plot) or width (right of it) it may take.
constexpr double kLegendGap = 8.0;
// The color scale's key: the bar's width, and the room between it and the axis left of it.
constexpr double kColorBarWidth  = 12.0;
constexpr double kColorKeyGap    = 16.0;
constexpr double kMaxLegendShare = 0.4;

// The ranges a layout uses: the axes' own, or overrides of them.
struct Ranges
{
    Range x;
    Range y;
    Range y2;
};

Ranges rangesOf(const PlotWidget& plot, const RangeOverrides& overrides)
{
    return {.x  = overrides.x.value_or(plot.xAxis()->range()),
            .y  = overrides.y.value_or(plot.yAxis()->range()),
            .y2 = overrides.y2.value_or(plot.yAxis2()->range())};
}

double widestLabel(const QStringList& labels, const QFontMetricsF& metrics)
{
    double widest = 0.0;
    for (const QString& label : labels)
    {
        widest = std::max(widest, metrics.horizontalAdvance(label));
    }
    return widest;
}

// Rounds to whole device pixels, so plot edges and the lines drawn on them are crisp.
double snap(double value, double devicePixelRatio)
{
    return std::round(value * devicePixelRatio) / devicePixelRatio;
}

core::TickKind kindOf(const Axis& axis)
{
    switch (axis.scaleType())
    {
        case ScaleType::LINEAR:
            return core::TickKind::LINEAR;
        case ScaleType::LOGARITHMIC:
            return core::TickKind::LOG;
        case ScaleType::DATE_TIME:
            return core::TickKind::TIME;
        case ScaleType::CATEGORY:
            return core::TickKind::CATEGORY;
    }
    return core::TickKind::LINEAR;
}

core::NumberStyle styleOf(NumberFormat format)
{
    switch (format)
    {
        case NumberFormat::AUTO:
            return core::NumberStyle::AUTO;
        case NumberFormat::SI:
            return core::NumberStyle::SI;
        case NumberFormat::PLAIN:
            return core::NumberStyle::PLAIN;
    }
    return core::NumberStyle::AUTO;
}

core::UtcOffset utcOffsetOf(const QTimeZone& zone)
{
    if (zone == QTimeZone::utc())
    {
        return {};
    }
    return
        [zone](double utc) { return static_cast<double>(zone.offsetFromUtc(fromPlotTime(utc))); };
}

QString zoneNameOf(const QTimeZone& zone, double at)
{
    if (zone == QTimeZone::utc())
    {
        return QStringLiteral("UTC");
    }
    const QString abbreviation = zone.abbreviation(fromPlotTime(at));
    return abbreviation.isEmpty() ? zone.displayName(fromPlotTime(at), QTimeZone::OffsetName)
                                  : abbreviation;
}

// The names of @p axis's categories, for the core.
std::vector<std::string> categoriesOf(const Axis& axis)
{
    std::vector<std::string> names;
    if (axis.scaleType() == ScaleType::CATEGORY)
    {
        const QStringList categories = axis.categories();
        names.reserve(static_cast<std::size_t>(categories.size()));
        for (const QString& name : categories)
        {
            names.push_back(name.toStdString());
        }
    }
    return names;
}

// @p value written in full for @p axis, to @p resolution (data units).
QString formatAxisValue(const Axis& axis, double value, double resolution)
{
    const bool                     time       = axis.scaleType() == ScaleType::DATE_TIME;
    const std::vector<std::string> categories = categoriesOf(axis);
    return QString::fromStdString(
        core::formatReadout(value, resolution, kindOf(axis), styleOf(axis.numberFormat()),
                            time ? utcOffsetOf(axis.timeZone()) : core::UtcOffset{}, categories));
}

// The height a y tick label of @p axis needs, from the height of a line of tick labels.
double yLabelSpacing(const Axis& axis, double lineHeight)
{
    return lineHeight *
           (axis.scaleType() == ScaleType::CATEGORY ? kYCategoryLabelSpacing : kYLabelSpacing);
}

// Fills ticks, labels and annotation for @p axis showing @p range over @p lengthPx.
void fillTicks(AxisLayout& layout, const Axis& axis, Range range, double lengthPx,
               double minSpacingPx)
{
    const bool                     time       = axis.scaleType() == ScaleType::DATE_TIME;
    const std::vector<std::string> categories = categoriesOf(axis);
    const core::TickRequest        request{
        .range             = range,
        .kind              = kindOf(axis),
        .style             = styleOf(axis.numberFormat()),
        .lengthPx          = lengthPx,
        .minSpacingPx      = minSpacingPx,
        .minMinorSpacingPx = kMinorSpacing,
        .utcOffset         = time ? utcOffsetOf(axis.timeZone()) : core::UtcOffset{},
        .zoneName   = time ? zoneNameOf(axis.timeZone(), range.min).toStdString() : std::string(),
        .categories = categories,
    };
    core::AxisTicks ticks = core::makeTicks(request);
    layout.major          = std::move(ticks.major);
    layout.minor          = std::move(ticks.minor);
    layout.labels.clear();
    for (const std::string& label : ticks.labels)
    {
        layout.labels.append(QString::fromStdString(label));
    }
    layout.annotation = QString::fromStdString(ticks.annotation);
}

// The smallest distance in pixels between neighboring major ticks.
double closestTicks(const AxisLayout& layout, const core::AxisMapping& mapping)
{
    double closest = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < layout.major.size(); ++i)
    {
        closest = std::min(closest, std::abs(mapping.toPixel(layout.major[i]) -
                                             mapping.toPixel(layout.major[i - 1])));
    }
    return closest;
}

void setFonts(PlotLayout& layout, const QFont& plotFont, const Theme& theme)
{
    const QFont font = grayAntialiased(plotFont);
    layout.tickFont  = scaledFont(font, theme.tickFontScale);
    layout.tickFont.setFeature(QFont::Tag("tnum"),
                               1);  // digits of equal width: labels don't jitter
    layout.labelFont = scaledFont(font, theme.labelFontScale);
    layout.titleFont = scaledFont(font, theme.titleFontScale);
    layout.titleFont.setWeight(QFont::DemiBold);
    layout.subtitleFont   = scaledFont(font, theme.subtitleFontScale);
    layout.legendFont     = layout.labelFont;
    layout.annotationFont = scaledFont(font, theme.annotationFontScale);
}

double tickLabelHeight(const PlotLayout& layout)
{
    return QFontMetricsF(layout.tickFont).height();
}

// Whether @p axis, shown, needs a row for an annotation (offset, multiplier, date): a guess from
// ticks over a rough share of the target.
bool needsAnnotation(const Axis& axis, Range range, bool shown, double length, double spacing)
{
    if (!shown)
    {
        return false;
    }
    AxisLayout probe;
    fillTicks(probe, axis, range, length, spacing);
    return !probe.annotation.isEmpty();
}

// The vertical stack: title, subtitle, the y axes' annotation row, the band of event flags (if
// any), the plot, the x axis, the x label and annotation row.
struct Vertical
{
    double plotTop       = 0.0;
    double plotBottom    = 0.0;
    double annotationTop = 0.0;  // of the y axes' annotations
    double xRowTop       = 0.0;  // of the x label and annotation
    double xRow          = 0.0;
    double xAxisHeight   = 0.0;
    double flagBand      = 0.0;  // the band's height, above the plot area and its gap
    double legendTop     = 0.0;  // of a legend under the plot
};

Vertical layoutVertically(const PlotWidget& plot, PlotLayout& layout, TextPainter& text,
                          const LayoutConstraints& constraints, const Ranges& ranges,
                          double flagBand, double legendHeight)
{
    const QRectF& bounds      = layout.bounds;
    const double  tickHeight  = tickLabelHeight(layout);
    const double  probeHeight = bounds.height() * kProbeShare;
    const bool    xAnnotation =
        needsAnnotation(*plot.xAxis(), ranges.x, layout.x.shown && layout.x.labeled,
                        bounds.width() * kProbeShare, kProbeSpacing);
    const bool yAnnotations =
        needsAnnotation(*plot.yAxis(), ranges.y, layout.y.shown && layout.y.labeled, probeHeight,
                        yLabelSpacing(*plot.yAxis(), tickHeight)) ||
        needsAnnotation(*plot.yAxis2(), ranges.y2, layout.y2.shown && layout.y2.labeled,
                        probeHeight, yLabelSpacing(*plot.yAxis2(), tickHeight));

    Vertical vertical;
    double   top = bounds.top() + kOuterPadding;
    if (!plot.title().isEmpty())
    {
        const double height = text.size(plot.title(), layout.titleFont).height();
        layout.title        = QRectF(bounds.left(), top, bounds.width(), height);
        top += height;
    }
    if (!plot.subtitle().isEmpty())
    {
        top += layout.title.isNull() ? 0.0 : kSubtitleGap;
        const double height = text.size(plot.subtitle(), layout.subtitleFont).height();
        layout.subtitle     = QRectF(bounds.left(), top, bounds.width(), height);
        top += height;
    }
    if (!layout.title.isNull() || !layout.subtitle.isNull())
    {
        top += kTitleGap;
    }
    vertical.annotationTop = top;
    // Without annotations, room for the top y label, which sticks out above the plot.
    top += yAnnotations ? tickHeight + kAnnotationGap : tickHeight / 2.0;
    if (flagBand > 0.0)
    {
        vertical.flagBand = flagBand;
        top += flagBand + kBandGap;
    }

    const QSizeF xTitle =
        layout.x.shown ? text.size(plot.xAxis()->label(), layout.labelFont) : QSizeF();
    double bottom = bounds.bottom() - kOuterPadding;
    if (legendHeight > 0.0)  // a legend at the very bottom
    {
        vertical.legendTop = bottom - legendHeight;
        bottom             = vertical.legendTop - kLegendGap;
    }
    vertical.xRow    = std::max(xTitle.height(), xAnnotation ? tickHeight : 0.0);
    vertical.xRowTop = bottom - vertical.xRow;
    if (vertical.xRow > 0.0)
    {
        bottom -= vertical.xRow + kAxisLabelGap;
    }
    if (layout.x.shown)
    {
        vertical.xAxisHeight =
            plot.theme().tickLength + (layout.x.labeled ? kTickLabelGap + tickHeight : 0.0);
    }
    vertical.plotTop    = snap(top, layout.devicePixelRatio);
    vertical.plotBottom = snap(bottom - vertical.xAxisHeight, layout.devicePixelRatio);

    // Plots side by side in a grid line up: each leaves the room the one with the most above and
    // below its plot area needs, and what sits there moves with the plot area.
    layout.naturalTop        = vertical.plotTop - bounds.top();
    layout.naturalBottom     = bounds.bottom() - vertical.plotBottom;
    const double extraTop    = std::max(0.0, constraints.minTop - layout.naturalTop);
    const double extraBottom = std::max(0.0, constraints.minBottom - layout.naturalBottom);
    vertical.plotTop         = snap(vertical.plotTop + extraTop, layout.devicePixelRatio);
    vertical.annotationTop += extraTop;
    vertical.plotBottom = snap(vertical.plotBottom - extraBottom, layout.devicePixelRatio);
    vertical.xRowTop -= extraBottom;
    return vertical;
}

// The y axis: its ticks, and how far right of the target's left edge it reaches (title, labels,
// ticks). Returns the left edge of the plot area it leaves.
double layoutLeftAxis(const PlotWidget& plot, PlotLayout& layout, TextPainter& text, Range range,
                      double plotTop, double plotHeight)
{
    double left = layout.bounds.left() + kOuterPadding;
    if (!layout.y.shown)
    {
        return left;
    }
    const QFontMetricsF metrics(layout.tickFont);
    fillTicks(layout.y, *plot.yAxis(), range, plotHeight,
              yLabelSpacing(*plot.yAxis(), metrics.height()));
    const QSizeF title = text.size(plot.yAxis()->label(), layout.labelFont);
    if (!title.isEmpty())
    {
        layout.y.titleRect = QRectF(left, plotTop, title.height(), plotHeight);
        left += title.height() + kAxisLabelGap;
    }
    const double labels =
        layout.y.labeled ? widestLabel(layout.y.labels, metrics) + kTickLabelGap : 0.0;
    return left + labels + plot.theme().tickLength;
}

// The same for the secondary y axis on the right. Returns the right edge of the plot area.
double layoutRightAxis(const PlotWidget& plot, PlotLayout& layout, TextPainter& text, Range range,
                       double plotTop, double plotHeight, double right)
{
    if (!layout.y2.shown)
    {
        return right;
    }
    const QFontMetricsF metrics(layout.tickFont);
    fillTicks(layout.y2, *plot.yAxis2(), range, plotHeight,
              yLabelSpacing(*plot.yAxis2(), metrics.height()));
    const QSizeF title = text.size(plot.yAxis2()->label(), layout.labelFont);
    if (!title.isEmpty())
    {
        layout.y2.titleRect = QRectF(right - title.height(), plotTop, title.height(), plotHeight);
        right -= title.height() + kAxisLabelGap;
    }
    const double labels =
        layout.y2.labeled ? widestLabel(layout.y2.labels, metrics) + kTickLabelGap : 0.0;
    return right - labels - plot.theme().tickLength;
}

// The key of the color scale, if it is shown, from @p right leftward: its title, labels, ticks and
// bar. Returns the right edge of what is left of it.
double layoutColorKey(const PlotWidget& plot, PlotLayout& layout, TextPainter& text, double plotTop,
                      double plotHeight, double right)
{
    const ColorScale& scale = *plot.colorScale();
    if (!scale.isVisible())
    {
        return right;
    }
    layout.colorKeyShown      = true;
    AxisLayout&           key = layout.colorKey;
    const QFontMetricsF   metrics(layout.tickFont);
    const core::AxisTicks ticks = core::makeTicks({
        .range        = scale.range(),
        .kind         = core::TickKind::LINEAR,
        .style        = styleOf(scale.numberFormat()),
        .lengthPx     = plotHeight,
        .minSpacingPx = metrics.height() * kYLabelSpacing,
    });
    key.shown                   = true;
    key.major                   = ticks.major;
    key.labels.clear();
    for (const std::string& label : ticks.labels)
    {
        key.labels.append(QString::fromStdString(label));
    }
    key.annotation     = QString::fromStdString(ticks.annotation);
    const QSizeF title = text.size(colorKeyTitle(scale.label(), key.annotation), layout.labelFont);
    if (!title.isEmpty())
    {
        key.titleRect = QRectF(right - title.height(), plotTop, title.height(), plotHeight);
        right -= title.height() + kAxisLabelGap;
    }
    const double labelsRight = right;
    right -= widestLabel(key.labels, metrics) + kTickLabelGap + plot.theme().tickLength;
    const double barRight = snap(right, layout.devicePixelRatio);
    const double barLeft  = snap(right - kColorBarWidth, layout.devicePixelRatio);
    layout.colorBar       = QRectF(barLeft, plotTop, barRight - barLeft, plotHeight);
    key.area              = QRectF(barRight, plotTop, labelsRight - barRight, plotHeight);
    key.mapping           = core::AxisMapping(scale.range(), plotTop + plotHeight, plotTop);
    return barLeft - kColorKeyGap;
}

// The x ticks, spaced by their labels' width, with the last label kept left of @p limit (which
// may move the plot's right edge in). Returns the plot area's right edge.
double layoutXTicks(const Axis& xAxis, Range range, PlotLayout& layout, double plotLeft,
                    double right, double limit)
{
    const QFontMetricsF metrics(layout.tickFont);
    const QRectF&       bounds = layout.bounds;
    double minSpacing = metrics.horizontalAdvance(QStringLiteral("\u22120.000")) + kXLabelSpacing;
    double plotRight  = snap(right, layout.devicePixelRatio);
    for (int attempt = 0; layout.x.shown && attempt < kXLayoutAttempts; ++attempt)
    {
        plotRight = snap(right, layout.devicePixelRatio);
        if (plotRight - plotLeft < kMinPlotSize)
        {
            break;
        }
        fillTicks(layout.x, xAxis, range, plotRight - plotLeft, minSpacing);
        const core::AxisMapping mapping(range, plotLeft, plotRight, scaleOf(xAxis));
        double                  overflow = 0.0;
        if (layout.x.labeled && !layout.x.major.empty())
        {
            const double lastRightEdge = mapping.toPixel(layout.x.major.back()) +
                                         (metrics.horizontalAdvance(layout.x.labels.back()) / 2.0);
            overflow                   = lastRightEdge - limit;
        }
        const double needed = widestLabel(layout.x.labels, metrics) + kXLabelSpacing;
        if (closestTicks(layout.x, mapping) < needed)
        {
            minSpacing = needed;
        }
        else if (overflow > 0.5)
        {
            right -= overflow;
            layout.naturalRight = std::max(layout.naturalRight, bounds.right() - right);
        }
        else
        {
            break;
        }
    }
    return plotRight;
}

// Mappings and the rectangles around the plot area for every axis.
void placeAxes(const PlotWidget& plot, PlotLayout& layout, const Vertical& vertical,
               const Ranges& ranges)
{
    const QRectF& area   = layout.plot;
    const QRectF& bounds = layout.bounds;
    const Axis&   x      = *plot.xAxis();
    const Axis&   y      = *plot.yAxis();
    const Axis&   y2     = *plot.yAxis2();
    layout.x.mapping     = core::AxisMapping(ranges.x, area.left(), area.right(), scaleOf(x));
    layout.y.mapping     = core::AxisMapping(ranges.y, area.bottom(), area.top(), scaleOf(y));
    layout.y2.mapping    = core::AxisMapping(ranges.y2, area.bottom(), area.top(), scaleOf(y2));

    const double tickHeight   = tickLabelHeight(layout);
    const double contentLeft  = bounds.left() + kOuterPadding;
    const double contentRight = bounds.right() - kOuterPadding;
    layout.x.area = QRectF(area.left(), area.bottom(), area.width(), vertical.xAxisHeight);
    if (layout.x.shown && !x.label().isEmpty())
    {
        layout.x.titleRect = QRectF(area.left(), vertical.xRowTop, area.width(), vertical.xRow);
    }
    layout.x.annotationRect =
        QRectF(area.left(), vertical.xRowTop, contentRight - area.left(), vertical.xRow);

    const double yAreaLeft = layout.y.titleRect.isNull() ? contentLeft : layout.y.titleRect.right();
    layout.y.area          = QRectF(yAreaLeft, area.top(), area.left() - yAreaLeft, area.height());
    layout.y.annotationRect =
        QRectF(contentLeft, vertical.annotationTop, area.center().x() - contentLeft, tickHeight);
    // The right axis reaches as far as its title, or the color key, or the edge.
    double y2AreaRight = contentRight;
    if (!layout.y2.titleRect.isNull())
    {
        y2AreaRight = layout.y2.titleRect.left();
    }
    else if (layout.colorKeyShown)
    {
        y2AreaRight = layout.colorBar.left() - (kColorKeyGap / 2.0);
    }
    layout.y2.area = QRectF(area.right(), area.top(), y2AreaRight - area.right(), area.height());
    layout.y2.annotationRect = QRectF(area.center().x(), vertical.annotationTop,
                                      contentRight - area.center().x(), tickHeight);

    if (vertical.flagBand > 0.0)
    {
        layout.flagBand = QRectF(area.left(), area.top() - kBandGap - vertical.flagBand,
                                 area.width(), vertical.flagBand);
    }

    // The title and subtitle are centered over the plot area, not the whole widget.
    for (QRectF* heading : {&layout.title, &layout.subtitle})
    {
        if (!heading->isNull())
        {
            heading->setLeft(area.left());
            heading->setRight(area.right());
        }
    }
}

}  // namespace

bool hasSeriesOnSecondary(const PlotWidget& plot)
{
    return std::ranges::any_of(plot.series(),
                               [](const Series* series) { return series->isOnSecondaryYAxis(); });
}

QFont scaledFont(const QFont& base, double scale)
{
    QFont font = base;
    if (base.pointSizeF() > 0.0)
    {
        font.setPointSizeF(base.pointSizeF() * scale);
    }
    else if (base.pixelSize() > 0)
    {
        font.setPixelSize(std::max(1, static_cast<int>(std::lround(base.pixelSize() * scale))));
    }
    return font;
}

QFont grayAntialiased(QFont font)
{
    font.setStyleStrategy(
        static_cast<QFont::StyleStrategy>(font.styleStrategy() | QFont::NoSubpixelAntialias));
    return font;
}

const AxisLayout& PlotLayout::yFor(const Series& series) const
{
    return series.isOnSecondaryYAxis() ? y2 : y;
}

const AxisLayout& PlotLayout::yFor(const Annotation& annotation) const
{
    return annotation.isOnSecondaryYAxis() ? y2 : y;
}

AnnotationMappings PlotLayout::mappingsFor(const Annotation& annotation) const
{
    if (annotation.coordinates() == AnnotationCoordinates::PLOT_AREA)
    {
        constexpr Range kFractions{.min = 0.0, .max = 1.0};
        return {.x = core::AxisMapping(kFractions, plot.left(), plot.right()),
                .y = core::AxisMapping(kFractions, plot.bottom(), plot.top())};
    }
    return {.x = x.mapping, .y = yFor(annotation).mapping};
}

core::Scale scaleOf(const Axis& axis)
{
    return axis.scaleType() == ScaleType::LOGARITHMIC ? core::Scale::LOG : core::Scale::LINEAR;
}

QString colorKeyTitle(const QString& label, const QString& annotation)
{
    if (annotation.isEmpty())
    {
        return label;
    }
    return label.isEmpty() ? annotation : label + QStringLiteral("  ") + annotation;
}

QString readoutLabel(const Axis& axis, const core::AxisMapping& mapping, double pixel)
{
    const double value = mapping.toValue(pixel);
    return formatAxisValue(axis, value, std::abs(mapping.toValue(pixel + 1.0) - value));
}

QString valueLabel(const Axis& axis, const core::AxisMapping& mapping, double value)
{
    const double pixel = mapping.toPixel(value);
    return formatAxisValue(axis, value, std::abs(mapping.toValue(pixel + 1.0) - value));
}

namespace
{

// One layout, with a band of @p flagBand pixels for event flags above the plot area (0: none).
PlotLayout layoutOnce(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                      double devicePixelRatio, TextPainter& text,
                      const LayoutConstraints& constraints, const Ranges& ranges, double flagBand)
{
    PlotLayout layout;
    layout.bounds           = bounds;
    layout.devicePixelRatio = devicePixelRatio;
    setFonts(layout, font, plot.theme());
    layout.x.shown    = plot.xAxis()->isShown(true);
    layout.y.shown    = plot.yAxis()->isShown(true);
    layout.y2.shown   = plot.yAxis2()->isShown(hasSeriesOnSecondary(plot));
    layout.x.labeled  = plot.xAxis()->areTickLabelsVisible();
    layout.y.labeled  = plot.yAxis()->areTickLabelsVisible();
    layout.y2.labeled = plot.yAxis2()->areTickLabelsVisible();

    // A legend under the plot, as tall as its rows across the width.
    const LegendAnchor anchor = plot.legend()->anchor();
    if (anchor == LegendAnchor::OUTSIDE_BOTTOM)
    {
        layout.legend =
            measureOutsideLegend(plot, layout, text, bounds.width() - (2.0 * kOuterPadding),
                                 bounds.height() * kMaxLegendShare);
    }
    const Vertical vertical = layoutVertically(plot, layout, text, constraints, ranges, flagBand,
                                               layout.legend.box.height());
    if (vertical.plotBottom - vertical.plotTop < kMinPlotSize)
    {
        return layout;
    }
    const double plotHeight = vertical.plotBottom - vertical.plotTop;
    // A legend right of the plot, in columns as tall as the plot area.
    double right = bounds.right() - kOuterPadding;
    double limit = bounds.right() - (kOuterPadding / 2.0);  // for the last x label
    if (anchor == LegendAnchor::OUTSIDE_RIGHT)
    {
        layout.legend =
            measureOutsideLegend(plot, layout, text, plotHeight, bounds.width() * kMaxLegendShare);
        if (!layout.legend.box.isNull())
        {
            layout.legend.box.moveTopRight(QPointF(right, vertical.plotTop));
            right = layout.legend.box.left() - kLegendGap;
            limit = right + (kLegendGap / 2.0);
        }
    }
    const double left = layoutLeftAxis(plot, layout, text, ranges.y, vertical.plotTop, plotHeight);
    right             = layoutColorKey(plot, layout, text, vertical.plotTop, plotHeight, right);
    right = layoutRightAxis(plot, layout, text, ranges.y2, vertical.plotTop, plotHeight, right);
    layout.naturalLeft  = left - bounds.left();
    layout.naturalRight = bounds.right() - right;
    const double plotLeft =
        snap(std::max(left, bounds.left() + constraints.minLeft), devicePixelRatio);
    right                  = std::min(right, bounds.right() - constraints.minRight);
    const double plotRight = layoutXTicks(*plot.xAxis(), ranges.x, layout, plotLeft, right, limit);
    if (plotRight - plotLeft < kMinPlotSize)
    {
        return layout;
    }

    layout.plot = QRectF(plotLeft, vertical.plotTop, plotRight - plotLeft, plotHeight);
    placeAxes(plot, layout, vertical, ranges);
    if (anchor == LegendAnchor::OUTSIDE_BOTTOM && !layout.legend.box.isNull())
    {
        // Centered under the plot area, inside the target.
        const double width      = layout.legend.box.width();
        const double legendLeft = std::clamp(
            layout.plot.center().x() - (width / 2.0), bounds.left() + kOuterPadding,
            std::max(bounds.left() + kOuterPadding, bounds.right() - kOuterPadding - width));
        layout.legend.box.moveTopLeft(QPointF(legendLeft, vertical.legendTop));
    }
    layout.valid = true;
    return layout;
}

}  // namespace

PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                      double devicePixelRatio, TextPainter& text, LayoutConstraints constraints,
                      const RangeOverrides& overrides)
{
    const Ranges          ranges      = rangesOf(plot, overrides);
    const Qt::Orientation orientation = plot.eventFlagOrientation();
    if (plot.eventFlagPlacement() != FlagPlacement::ABOVE_PLOT_AREA)
    {
        PlotLayout layout =
            layoutOnce(plot, bounds, font, devicePixelRatio, text, constraints, ranges, 0.0);
        if (layout.valid)
        {
            layout.flags = layoutEventFlags(plot, layout, text, layout.plot.top() + kFlagTop,
                                            layout.plot.height() * kFlagRowShare, orientation);
        }
        return layout;
    }
    // Flags in a band above the plot area, as tall as their rows: which rows they take depends on
    // the x axis, which depends on the band's height (through the y labels' width). A few layouts
    // settle it.
    const double maxBand = bounds.height() * kMaxBandShare;
    double       band    = 0.0;
    PlotLayout   layout;
    for (int pass = 0; pass < kBandPasses; ++pass)
    {
        layout = layoutOnce(plot, bounds, font, devicePixelRatio, text, constraints, ranges, band);
        if (!layout.valid)
        {
            return layout;
        }
        const double top    = layout.plot.top() - kBandGap - band;
        layout.flags        = layoutEventFlags(plot, layout, text, top, maxBand, orientation);
        const double needed = std::min(layout.flags.height, maxBand);
        if (std::abs(needed - band) < 0.5)
        {
            return layout;
        }
        band = needed;
    }
    // Not settled: the flags that fit the band of the last layout.
    layout = layoutOnce(plot, bounds, font, devicePixelRatio, text, constraints, ranges, band);
    if (layout.valid)
    {
        layout.flags = layoutEventFlags(plot, layout, text, layout.flagBand.top(),
                                        layout.flagBand.height(), orientation);
    }
    return layout;
}

}  // namespace rocketplot
