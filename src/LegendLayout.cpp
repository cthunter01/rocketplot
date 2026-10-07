#include "LegendLayout.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "MarkerPainter.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "core/Occupancy.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr double kMargin        = 10.0;  // from the plot area's edges
constexpr double kPadding       = 8.0;
constexpr double kSwatch        = 24.0;
constexpr double kSwatchGap     = 8.0;
constexpr double kRowGap        = 4.0;
constexpr double kValueGap      = 16.0;  // between a name and its value
constexpr double kRadius        = 4.0;
constexpr double kRowRadius     = 3.0;
constexpr double kHiddenOpacity = 0.35;
constexpr double kMaxSwatchLine = 3.0;
// A swatch's errors: the height of a patch of band, the length of a bar and the width of its caps.
constexpr double kSwatchBand = 10.0;
constexpr double kSwatchBar  = 14.0;
constexpr double kSwatchCap  = 5.0;
// A BEST legend moves only to a spot where it covers less data by this share of its area (a line
// crossing it covers a few percent).
constexpr double kStickiness = 0.01;
// The spots a BEST legend tries, the first preferred on a tie (matplotlib's order).
constexpr std::array kBestOrder = {
    LegendAnchor::TOP_RIGHT,    LegendAnchor::TOP_LEFT, LegendAnchor::BOTTOM_LEFT,
    LegendAnchor::BOTTOM_RIGHT, LegendAnchor::RIGHT,    LegendAnchor::LEFT,
    LegendAnchor::BOTTOM,       LegendAnchor::TOP,
};

QFont valueFont(const PlotLayout& layout)
{
    QFont font = layout.legendFont;
    font.setFeature(QFont::Tag("tnum"), 1);  // digits of equal width: values don't jitter
    return font;
}

// @p series' value at the crosshair: the y of @p point, the one of its points the crosshair is
// on, or without one the y of its point nearest @p x. Empty when it can't tell (not sorted by x),
// a dash where the series has none.
QString valueOf(const Series& series, const PlotLayout& layout, double x,
                std::optional<std::size_t> point)
{
    if (!point && !series.isSortedByX())
    {
        return {};
    }
    const std::optional<std::size_t> index = point ? point : series.nearestIndex(x);
    const double y = index ? series.y(*index) : std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(y))
    {
        return QStringLiteral("—");
    }
    return valueLabel(*series.yAxis(), layout.yFor(series).mapping, y);
}

LegendAnchor bestAnchor(const QRectF& plotArea, QSizeF size, const core::Occupancy& occupancy,
                        std::optional<LegendAnchor> previous)
{
    LegendAnchor best             = kBestOrder.front();
    double       bestCoverage     = std::numeric_limits<double>::infinity();
    double       previousCoverage = std::numeric_limits<double>::infinity();
    for (const LegendAnchor anchor : kBestOrder)
    {
        const QRectF box      = legendRect(plotArea, size, anchor);
        const double coverage = occupancy.coverage({
            .left   = box.left(),
            .top    = box.top(),
            .right  = box.right(),
            .bottom = box.bottom(),
        });
        if (coverage < bestCoverage)
        {
            best         = anchor;
            bestCoverage = coverage;
        }
        if (anchor == previous)
        {
            previousCoverage = coverage;
        }
    }
    return previous && previousCoverage <= bestCoverage + kStickiness ? *previous : best;
}

// What a series' errors look like, behind its line and marker: a patch of its band, or a bar with
// caps.
void drawErrorSwatch(QPainter& painter, const Series& series, const Theme& theme, QPointF center)
{
    if (!series.hasXErrors() && !series.hasYErrors())
    {
        return;
    }
    QColor color = series.color();
    if (series.hasYErrors() && series.errorStyle() == ErrorStyle::BAND && series.isSortedByX())
    {
        color.setAlphaF(
            static_cast<float>(static_cast<double>(color.alphaF()) * series.bandOpacity()));
        painter.fillRect(QRectF(center.x() - (kSwatch / 2.0), center.y() - (kSwatchBand / 2.0),
                                kSwatch, kSwatchBand),
                         color);
        return;
    }
    // Upright for y errors, lying down for x errors alone.
    const QPointF reach =
        series.hasYErrors() ? QPointF(0.0, kSwatchBar / 2.0) : QPointF(kSwatchBar / 2.0, 0.0);
    const QPointF cap = QPointF(reach.y(), reach.x()) * (kSwatchCap / kSwatchBar);
    painter.setPen(QPen(color, theme.errorBarWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(center - reach, center + reach);
    painter.drawLine(center - reach - cap, center - reach + cap);
    painter.drawLine(center + reach - cap, center + reach + cap);
}

// A line (for line series) with the series' marker across the middle, over its errors.
void drawSwatch(QPainter& painter, const Series& series, const Theme& theme, QPointF center)
{
    drawErrorSwatch(painter, series, theme, center);
    if (const auto* line = qobject_cast<const LineSeries*>(&series))
    {
        QPen pen = line->pen();
        pen.setWidthF(std::min(pen.widthF(), kMaxSwatchLine));
        painter.setPen(pen);
        painter.drawLine(center - QPointF(kSwatch / 2.0, 0.0),
                         center + QPointF(kSwatch / 2.0, 0.0));
    }
    MarkerPainter::draw(painter, center, markerStyle(series, theme));
}

}  // namespace

const LegendEntry* LegendLayout::entryAt(QPointF position) const
{
    if (!box.contains(position))
    {
        return nullptr;
    }
    const auto entry = std::ranges::find_if(
        entries, [&](const LegendEntry& candidate) { return candidate.row.contains(position); });
    return entry == entries.end() ? nullptr : &*entry;
}

QRectF legendRect(const QRectF& plotArea, QSizeF size, LegendAnchor anchor, QPointF position)
{
    const double width   = size.width();
    const double height  = size.height();
    const double left    = plotArea.left() + kMargin;
    const double right   = plotArea.right() - kMargin - width;
    const double centerX = plotArea.center().x() - (width / 2.0);
    const double top     = plotArea.top() + kMargin;
    const double bottom  = plotArea.bottom() - kMargin - height;
    const double centerY = plotArea.center().y() - (height / 2.0);
    switch (anchor)
    {
        case LegendAnchor::TOP_LEFT:
            return {left, top, width, height};
        case LegendAnchor::TOP:
            return {centerX, top, width, height};
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
        case LegendAnchor::CUSTOM:
            // The room left around the legend, split as position says.
            return {left + (std::max(0.0, right - left) * position.x()),
                    top + (std::max(0.0, bottom - top) * position.y()), width, height};
        case LegendAnchor::BEST:
        case LegendAnchor::TOP_RIGHT:
            break;
    }
    return {right, top, width, height};
}

QPointF legendPosition(const QRectF& plotArea, QSizeF size, QPointF topLeft)
{
    const double roomX = plotArea.width() - (2.0 * kMargin) - size.width();
    const double roomY = plotArea.height() - (2.0 * kMargin) - size.height();
    const auto   share = [](double offset, double room) {
        return room > 0.0 ? std::clamp(offset / room, 0.0, 1.0) : 0.0;
    };
    return {share(topLeft.x() - plotArea.left() - kMargin, roomX),
            share(topLeft.y() - plotArea.top() - kMargin, roomY)};
}

LegendLayout layoutLegend(const PlotWidget& plot, const PlotLayout& layout, TextPainter& text,
                          const LegendState& state)
{
    LegendLayout legend;
    if (!layout.valid)
    {
        return legend;
    }
    for (Series* series : plot.series())
    {
        if (!series->name().isEmpty())
        {
            legend.entries.push_back(
                {.series = series, .name = series->name(), .value = {}, .row = {}});
        }
    }
    const Legend& options = *plot.legend();
    if (!options.isShownFor(static_cast<qsizetype>(legend.entries.size())))
    {
        legend.entries.clear();
        return legend;
    }

    double nameWidth  = 0.0;
    double nameHeight = QFontMetricsF(layout.legendFont).height();
    for (const LegendEntry& entry : legend.entries)
    {
        const QSizeF size = text.size(entry.name, layout.legendFont);
        nameWidth         = std::max(nameWidth, size.width());
        nameHeight        = std::max(nameHeight, size.height());
    }
    if (state.crosshairX && options.areValuesVisible())
    {
        const QFontMetricsF metrics(valueFont(layout));
        double              widest = 0.0;
        for (LegendEntry& entry : legend.entries)
        {
            if (entry.series->isVisible())
            {
                const bool onPoint = entry.series == state.crosshairSeries;
                entry.value = valueOf(*entry.series, layout, *state.crosshairX,
                                      onPoint ? std::optional(state.crosshairIndex) : std::nullopt);
                widest      = std::max(widest, metrics.horizontalAdvance(entry.value));
            }
        }
        legend.valueWidth = widest > 0.0 ? std::max(widest, state.minValueWidth) : 0.0;
    }

    const auto rows      = static_cast<double>(legend.entries.size());
    legend.rowHeight     = std::max(nameHeight, plot.theme().markerSize + 2.0);
    const double content = kSwatch + kSwatchGap + nameWidth +
                           (legend.valueWidth > 0.0 ? kValueGap + legend.valueWidth : 0.0);
    const QSizeF size(std::min(layout.plot.width() - (2.0 * kMargin), (2.0 * kPadding) + content),
                      (2.0 * kPadding) + (rows * legend.rowHeight) + ((rows - 1.0) * kRowGap));

    legend.anchor = options.anchor();
    if (legend.anchor == LegendAnchor::BEST)
    {
        legend.anchor = state.occupancy != nullptr && state.occupancy->isRecorded()
                            ? bestAnchor(layout.plot, size, *state.occupancy, state.previousBest)
                            : LegendAnchor::TOP_RIGHT;
    }
    legend.box = legendRect(layout.plot, size, legend.anchor, options.position());
    double top = legend.box.top() + kPadding;
    for (LegendEntry& entry : legend.entries)
    {
        entry.row = QRectF(legend.box.left(), top - (kRowGap / 2.0), legend.box.width(),
                           legend.rowHeight + kRowGap);
        top += legend.rowHeight + kRowGap;
    }
    return legend;
}

void drawLegend(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                const LegendLayout& legend, TextPainter& text, const Series* pointed)
{
    if (!legend.isShown())
    {
        return;
    }
    const Theme&  theme = plot.theme();
    const QRectF& box   = legend.box;
    const double  pixel = 1.0 / layout.devicePixelRatio;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(hairlinePen(theme.legendBorder));
    painter.setBrush(theme.legendBackground);
    painter.drawRoundedRect(box.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0),
                            kRadius, kRadius);

    const double textLeft   = box.left() + kPadding + kSwatch + kSwatchGap;
    const double valueRight = box.right() - kPadding;
    const double nameRight =
        legend.valueWidth > 0.0 ? valueRight - legend.valueWidth - kValueGap : valueRight;
    const QFont values = valueFont(layout);
    for (const LegendEntry& entry : legend.entries)
    {
        const QRectF row(box.left(), entry.row.center().y() - (legend.rowHeight / 2.0), box.width(),
                         legend.rowHeight);
        if (entry.series == pointed)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.legendBorder);
            painter.drawRoundedRect(entry.row.adjusted(kPadding / 2.0, 0.0, -kPadding / 2.0, 0.0),
                                    kRowRadius, kRowRadius);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setOpacity(entry.series->isVisible() ? 1.0 : kHiddenOpacity);
        drawSwatch(painter, *entry.series, theme,
                   QPointF(box.left() + kPadding + (kSwatch / 2.0), row.center().y()));
        text.draw(painter, entry.name, layout.legendFont, theme.text,
                  QRectF(textLeft, row.top(), std::max(0.0, nameRight - textLeft), row.height()),
                  Qt::AlignLeft | Qt::AlignVCenter, true);
        if (!entry.value.isEmpty())
        {
            painter.setFont(values);
            painter.setPen(theme.text);
            painter.drawText(
                QRectF(valueRight - legend.valueWidth, row.top(), legend.valueWidth, row.height()),
                static_cast<int>((Qt::AlignRight | Qt::AlignVCenter).toInt()), entry.value);
        }
    }
    painter.setOpacity(1.0);
    painter.restore();
}

}  // namespace rocketplot
