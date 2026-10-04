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
#include <utility>

#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/AxisTicks.h"
#include "core/NumberFormatter.h"
#include "core/TimeTicks.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"
#include "rocketplot/plottime.h"

namespace rocketplot
{

namespace
{

constexpr double kOuterPadding  = 10.0;
constexpr double kTitleGap      = 10.0;
constexpr double kAxisLabelGap  = 6.0;
constexpr double kAnnotationGap = 4.0;
constexpr double kMinPlotSize   = 16.0;
constexpr double kMinorSpacing  = 4.0;
// Space between neighboring x tick labels, and the height y tick labels need per tick (in label
// heights).
constexpr double kXLabelSpacing = 16.0;
constexpr double kYLabelSpacing = 2.2;
// Rounds of x tick layout: labels may turn out wider than guessed, or overflow the right edge.
constexpr int kXLayoutAttempts = 4;
// Rough share of the target the plot area gets, for the first guess at which axes need an
// annotation row.
constexpr double kProbeShare = 0.7;
// The x label spacing assumed by that guess.
constexpr double kProbeSpacing = 80.0;

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
    }
    return core::TickKind::LINEAR;
}

core::NumberStyle styleOf(const Axis& axis)
{
    switch (axis.numberFormat())
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

// @p value written in full for @p axis, to @p resolution (data units).
QString formatAxisValue(const Axis& axis, double value, double resolution)
{
    const bool time = axis.scaleType() == ScaleType::DATE_TIME;
    return QString::fromStdString(
        core::formatReadout(value, resolution, kindOf(axis), styleOf(axis),
                            time ? utcOffsetOf(axis.timeZone()) : core::UtcOffset{}));
}

// Fills ticks, labels and annotation for @p axis drawn over @p lengthPx.
void fillTicks(AxisLayout& layout, const Axis& axis, double lengthPx, double minSpacingPx)
{
    const bool              time = axis.scaleType() == ScaleType::DATE_TIME;
    const core::TickRequest request{
        .range             = axis.range(),
        .kind              = kindOf(axis),
        .style             = styleOf(axis),
        .lengthPx          = lengthPx,
        .minSpacingPx      = minSpacingPx,
        .minMinorSpacingPx = kMinorSpacing,
        .utcOffset         = time ? utcOffsetOf(axis.timeZone()) : core::UtcOffset{},
        .zoneName = time ? zoneNameOf(axis.timeZone(), axis.min()).toStdString() : std::string(),
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

bool hasSeriesOnSecondary(const PlotWidget& plot)
{
    return std::ranges::any_of(plot.series(),
                               [](const Series* series) { return series->isOnSecondaryYAxis(); });
}

void setFonts(PlotLayout& layout, const QFont& font, const Theme& theme)
{
    layout.tickFont = scaledFont(font, theme.tickFontScale);
    layout.tickFont.setFeature(QFont::Tag("tnum"),
                               1);  // digits of equal width: labels don't jitter
    layout.labelFont = scaledFont(font, theme.labelFontScale);
    layout.titleFont = scaledFont(font, theme.titleFontScale);
    layout.titleFont.setWeight(QFont::DemiBold);
    layout.legendFont     = layout.labelFont;
    layout.annotationFont = scaledFont(font, theme.annotationFontScale);
}

double tickLabelHeight(const PlotLayout& layout)
{
    return QFontMetricsF(layout.tickFont).height();
}

// Whether @p axis, shown, needs a row for an annotation (offset, multiplier, date): a guess from
// ticks over a rough share of the target.
bool needsAnnotation(const Axis& axis, bool shown, double length, double spacing)
{
    if (!shown)
    {
        return false;
    }
    AxisLayout probe;
    fillTicks(probe, axis, length, spacing);
    return !probe.annotation.isEmpty();
}

// The vertical stack: title, the y axes' annotation row, the plot, the x axis, the x label and
// annotation row.
struct Vertical
{
    double plotTop       = 0.0;
    double plotBottom    = 0.0;
    double annotationTop = 0.0;  // of the y axes' annotations
    double xRowTop       = 0.0;  // of the x label and annotation
    double xRow          = 0.0;
    double xAxisHeight   = 0.0;
};

Vertical layoutVertically(const PlotWidget& plot, PlotLayout& layout, TextPainter& text,
                          const LayoutConstraints& constraints)
{
    const QRectF& bounds      = layout.bounds;
    const double  tickHeight  = tickLabelHeight(layout);
    const double  probeHeight = bounds.height() * kProbeShare;
    const bool    xAnnotation = needsAnnotation(*plot.xAxis(), layout.x.shown && layout.x.labeled,
                                                bounds.width() * kProbeShare, kProbeSpacing);
    const bool yAnnotations = needsAnnotation(*plot.yAxis(), layout.y.shown && layout.y.labeled,
                                              probeHeight, tickHeight * kYLabelSpacing) ||
                              needsAnnotation(*plot.yAxis2(), layout.y2.shown && layout.y2.labeled,
                                              probeHeight, tickHeight * kYLabelSpacing);

    Vertical vertical;
    double   top = bounds.top() + kOuterPadding;
    if (!plot.title().isEmpty())
    {
        const double height = text.size(plot.title(), layout.titleFont).height();
        layout.title        = QRectF(bounds.left(), top, bounds.width(), height);
        top += height + kTitleGap;
    }
    vertical.annotationTop = top;
    // Without annotations, room for the top y label, which sticks out above the plot.
    top += yAnnotations ? tickHeight + kAnnotationGap : tickHeight / 2.0;

    const QSizeF xTitle =
        layout.x.shown ? text.size(plot.xAxis()->label(), layout.labelFont) : QSizeF();
    double bottom    = bounds.bottom() - kOuterPadding;
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
double layoutLeftAxis(const PlotWidget& plot, PlotLayout& layout, TextPainter& text, double plotTop,
                      double plotHeight)
{
    double left = layout.bounds.left() + kOuterPadding;
    if (!layout.y.shown)
    {
        return left;
    }
    const QFontMetricsF metrics(layout.tickFont);
    fillTicks(layout.y, *plot.yAxis(), plotHeight, metrics.height() * kYLabelSpacing);
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
double layoutRightAxis(const PlotWidget& plot, PlotLayout& layout, TextPainter& text,
                       double plotTop, double plotHeight)
{
    double right = layout.bounds.right() - kOuterPadding;
    if (!layout.y2.shown)
    {
        return right;
    }
    const QFontMetricsF metrics(layout.tickFont);
    fillTicks(layout.y2, *plot.yAxis2(), plotHeight, metrics.height() * kYLabelSpacing);
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

// The x ticks, spaced by their labels' width, with the last label kept inside the target (which
// may move the plot's right edge in). Returns the plot area's right edge.
double layoutXTicks(const Axis& xAxis, PlotLayout& layout, double plotLeft, double right)
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
        fillTicks(layout.x, xAxis, plotRight - plotLeft, minSpacing);
        const core::AxisMapping mapping(xAxis.range(), plotLeft, plotRight, scaleOf(xAxis));
        double                  overflow = 0.0;
        if (layout.x.labeled && !layout.x.major.empty())
        {
            const double lastRightEdge = mapping.toPixel(layout.x.major.back()) +
                                         (metrics.horizontalAdvance(layout.x.labels.back()) / 2.0);
            overflow                   = lastRightEdge - (bounds.right() - (kOuterPadding / 2.0));
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
void placeAxes(const PlotWidget& plot, PlotLayout& layout, const Vertical& vertical)
{
    const QRectF& area   = layout.plot;
    const QRectF& bounds = layout.bounds;
    const Axis&   x      = *plot.xAxis();
    const Axis&   y      = *plot.yAxis();
    const Axis&   y2     = *plot.yAxis2();
    layout.x.mapping     = core::AxisMapping(x.range(), area.left(), area.right(), scaleOf(x));
    layout.y.mapping     = core::AxisMapping(y.range(), area.bottom(), area.top(), scaleOf(y));
    layout.y2.mapping    = core::AxisMapping(y2.range(), area.bottom(), area.top(), scaleOf(y2));

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
    const double y2AreaRight =
        layout.y2.titleRect.isNull() ? contentRight : layout.y2.titleRect.left();
    layout.y2.area = QRectF(area.right(), area.top(), y2AreaRight - area.right(), area.height());
    layout.y2.annotationRect = QRectF(area.center().x(), vertical.annotationTop,
                                      contentRight - area.center().x(), tickHeight);

    // The title is centered over the plot area, not the whole widget.
    if (!layout.title.isNull())
    {
        layout.title.setLeft(area.left());
        layout.title.setRight(area.right());
    }
}

}  // namespace

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

const AxisLayout& PlotLayout::yFor(const Series& series) const
{
    return series.isOnSecondaryYAxis() ? y2 : y;
}

const AxisLayout& PlotLayout::yFor(const Annotation& annotation) const
{
    return annotation.isOnSecondaryYAxis() ? y2 : y;
}

core::Scale scaleOf(const Axis& axis)
{
    return axis.scaleType() == ScaleType::LOGARITHMIC ? core::Scale::LOG : core::Scale::LINEAR;
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

PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                      double devicePixelRatio, TextPainter& text, LayoutConstraints constraints)
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

    const Vertical vertical = layoutVertically(plot, layout, text, constraints);
    if (vertical.plotBottom - vertical.plotTop < kMinPlotSize)
    {
        return layout;
    }
    const double plotHeight = vertical.plotBottom - vertical.plotTop;
    const double left       = layoutLeftAxis(plot, layout, text, vertical.plotTop, plotHeight);
    double       right      = layoutRightAxis(plot, layout, text, vertical.plotTop, plotHeight);
    layout.naturalLeft      = left - bounds.left();
    layout.naturalRight     = bounds.right() - right;
    const double plotLeft =
        snap(std::max(left, bounds.left() + constraints.minLeft), devicePixelRatio);
    right                  = std::min(right, bounds.right() - constraints.minRight);
    const double plotRight = layoutXTicks(*plot.xAxis(), layout, plotLeft, right);
    if (plotRight - plotLeft < kMinPlotSize)
    {
        return layout;
    }

    layout.plot = QRectF(plotLeft, vertical.plotTop, plotRight - plotLeft, plotHeight);
    placeAxes(plot, layout, vertical);
    layout.valid = true;
    return layout;
}

}  // namespace rocketplot
