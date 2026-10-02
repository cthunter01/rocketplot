#include "PlotLayout.h"

#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cmath>

#include "core/AxisMapping.h"
#include "core/NumberFormatter.h"
#include "core/TickGenerator.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr double kOuterPadding = 10.0;
constexpr double kTitleGap     = 10.0;
constexpr double kAxisLabelGap = 6.0;
constexpr double kLegendMargin = 10.0;
constexpr double kMinPlotSize  = 16.0;
// Space between neighboring x tick labels, and the height y tick labels need per tick (in label
// heights).
constexpr double kXLabelSpacing = 16.0;
constexpr double kYLabelSpacing = 2.2;
// Rounds of x tick layout: labels may turn out wider than guessed, or overflow the right edge.
constexpr int kXLayoutAttempts = 4;

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

QStringList tickLabels(const core::Ticks& ticks, Range range)
{
    const core::TickFormat format =
        core::chooseTickFormat(ticks.step, std::max(std::abs(range.min), std::abs(range.max)));
    QStringList labels;
    labels.reserve(static_cast<qsizetype>(ticks.major.size()));
    for (const double value : ticks.major)
    {
        labels.append(QString::fromStdString(core::formatTick(value, format)));
    }
    return labels;
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

QRectF placeLegend(const QRectF& plot, double width, double height, LegendAnchor anchor)
{
    const double left    = plot.left() + kLegendMargin;
    const double right   = plot.right() - kLegendMargin - width;
    const double centerX = plot.center().x() - (width / 2.0);
    const double top     = plot.top() + kLegendMargin;
    const double bottom  = plot.bottom() - kLegendMargin - height;
    const double centerY = plot.center().y() - (height / 2.0);
    switch (anchor)
    {
        case LegendAnchor::TOP_LEFT:
            return {left, top, width, height};
        case LegendAnchor::TOP:
            return {centerX, top, width, height};
        case LegendAnchor::TOP_RIGHT:
            return {right, top, width, height};
        case LegendAnchor::RIGHT:
            return {right, centerY, width, height};
        case LegendAnchor::BOTTOM_RIGHT:
            return {right, bottom, width, height};
        case LegendAnchor::BOTTOM:
            return {centerX, bottom, width, height};
        case LegendAnchor::BOTTOM_LEFT:
            return {left, bottom, width, height};
        case LegendAnchor::LEFT:
            return {left, centerY, width, height};
    }
    return {right, top, width, height};
}

void layoutLegend(const PlotWidget& plot, PlotLayout& layout)
{
    for (const Series* series : plot.series())
    {
        if (!series->name().isEmpty())
        {
            layout.legendEntries.push_back({.series = series, .name = series->name()});
        }
    }
    if (!plot.legend()->isShownFor(static_cast<qsizetype>(layout.legendEntries.size())))
    {
        layout.legendEntries.clear();
        return;
    }
    const QFontMetricsF metrics(layout.legendFont);
    double              textWidth = 0.0;
    for (const LegendEntry& entry : layout.legendEntries)
    {
        textWidth = std::max(textWidth, metrics.horizontalAdvance(entry.name));
    }
    const auto rows        = static_cast<double>(layout.legendEntries.size());
    layout.legendRowHeight = std::max(metrics.height(), plot.theme().markerSize + 2.0);
    const double maxWidth  = layout.plot.width() - (2.0 * kLegendMargin);
    const double width =
        std::min(maxWidth, (2.0 * kLegendPadding) + kLegendSwatch + kLegendSwatchGap + textWidth);
    const double height =
        (2.0 * kLegendPadding) + (rows * layout.legendRowHeight) + ((rows - 1.0) * kLegendRowGap);
    layout.legend = placeLegend(layout.plot, width, height, plot.legend()->anchor());
}

}  // namespace

PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                      double devicePixelRatio)
{
    const Theme& theme = plot.theme();
    PlotLayout   layout;
    layout.bounds           = bounds;
    layout.devicePixelRatio = devicePixelRatio;
    layout.tickFont         = scaledFont(font, theme.tickFontScale);
    layout.tickFont.setFeature(QFont::Tag("tnum"),
                               1);  // digits of equal width: labels don't jitter
    layout.labelFont = scaledFont(font, theme.labelFontScale);
    layout.titleFont = scaledFont(font, theme.titleFontScale);
    layout.titleFont.setWeight(QFont::DemiBold);
    layout.legendFont = layout.labelFont;

    const QFontMetricsF tickMetrics(layout.tickFont);
    const QFontMetricsF labelMetrics(layout.labelFont);
    const QFontMetricsF titleMetrics(layout.titleFont);
    const Range         xRange = plot.xAxis()->range();
    const Range         yRange = plot.yAxis()->range();
    const QString       xLabel = plot.xAxis()->label();
    const QString       yLabel = plot.yAxis()->label();

    // Vertical: title, plot, x axis, x label.
    double top = bounds.top() + kOuterPadding;
    if (!plot.title().isEmpty())
    {
        layout.title = QRectF(bounds.left(), top, bounds.width(), titleMetrics.height());
        top += titleMetrics.height() + kTitleGap;
    }
    top += tickMetrics.height() / 2.0;  // the top y tick label sticks out above the plot

    double bottom = bounds.bottom() - kOuterPadding;
    if (!xLabel.isEmpty())
    {
        layout.xLabel = QRectF(bounds.left(), bottom - labelMetrics.height(), bounds.width(),
                               labelMetrics.height());
        bottom -= labelMetrics.height() + kAxisLabelGap;
    }
    const double plotTop = snap(top, devicePixelRatio);
    const double plotBottom =
        snap(bottom - theme.tickLength - kTickLabelGap - tickMetrics.height(), devicePixelRatio);
    if (plotBottom - plotTop < kMinPlotSize)
    {
        return layout;
    }

    // Horizontal: y label, y axis (as wide as its labels), plot. The y ticks only depend on the
    // plot's height.
    double left = bounds.left() + kOuterPadding;
    if (!yLabel.isEmpty())
    {
        layout.yLabel = QRectF(left, plotTop, labelMetrics.height(), plotBottom - plotTop);
        left += labelMetrics.height() + kAxisLabelGap;
    }
    layout.y.ticks =
        core::linearTicks(yRange, plotBottom - plotTop, tickMetrics.height() * kYLabelSpacing);
    layout.y.labels = tickLabels(layout.y.ticks, yRange);
    const double plotLeft =
        snap(left + widestLabel(layout.y.labels, tickMetrics) + kTickLabelGap + theme.tickLength,
             devicePixelRatio);

    // The x ticks: space them by their labels' width, and keep the last label inside the bounds.
    double right      = bounds.right() - kOuterPadding;
    double minSpacing = tickMetrics.horizontalAdvance(QStringLiteral("−0.000")) + kXLabelSpacing;
    double plotRight  = snap(right, devicePixelRatio);
    for (int attempt = 0; attempt < kXLayoutAttempts; ++attempt)
    {
        plotRight = snap(right, devicePixelRatio);
        if (plotRight - plotLeft < kMinPlotSize)
        {
            return layout;
        }
        layout.x.ticks         = core::linearTicks(xRange, plotRight - plotLeft, minSpacing);
        layout.x.labels        = tickLabels(layout.x.ticks, xRange);
        const double widest    = widestLabel(layout.x.labels, tickMetrics);
        const double needed    = widest + kXLabelSpacing;
        const double stepWidth = layout.x.ticks.step / xRange.span() * (plotRight - plotLeft);
        double       overflow  = 0.0;
        if (!layout.x.ticks.major.empty())
        {
            const core::AxisMapping mapping(xRange, plotLeft, plotRight);
            const double            lastRightEdge =
                mapping.toPixel(layout.x.ticks.major.back()) +
                (tickMetrics.horizontalAdvance(layout.x.labels.back()) / 2.0);
            overflow = lastRightEdge - (bounds.right() - (kOuterPadding / 2.0));
        }
        if (layout.x.ticks.major.size() > 1 && stepWidth < needed)
        {
            minSpacing = needed;
        }
        else if (overflow > 0.5)
        {
            right -= overflow;
        }
        else
        {
            break;
        }
    }

    layout.plot      = QRectF(plotLeft, plotTop, plotRight - plotLeft, plotBottom - plotTop);
    layout.x.mapping = core::AxisMapping(xRange, layout.plot.left(), layout.plot.right());
    layout.y.mapping = core::AxisMapping(yRange, layout.plot.bottom(), layout.plot.top());
    layout.xAxisArea = QRectF(layout.plot.left(), layout.plot.bottom(), layout.plot.width(),
                              theme.tickLength + kTickLabelGap + tickMetrics.height());
    layout.yAxisArea = QRectF(
        layout.yLabel.isNull() ? bounds.left() : layout.yLabel.right(), layout.plot.top(),
        layout.plot.left() - (layout.yLabel.isNull() ? bounds.left() : layout.yLabel.right()),
        layout.plot.height());
    // Title and x label are centered over the plot, not the whole widget.
    if (!layout.title.isNull())
    {
        layout.title.setLeft(layout.plot.left());
        layout.title.setRight(layout.plot.right());
    }
    if (!layout.xLabel.isNull())
    {
        layout.xLabel.setLeft(layout.plot.left());
        layout.xLabel.setRight(layout.plot.right());
    }
    layoutLegend(plot, layout);
    layout.valid = true;
    return layout;
}

}  // namespace rocketplot
