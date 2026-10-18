#include "LegendLayout.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringView>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "AnnotationPainter.h"
#include "MarkerPainter.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/LegendFlow.h"
#include "core/Occupancy.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/BarSeries.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/RectAnnotation.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/Series.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
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
constexpr double kColumnGap     = 16.0;  // between the columns of an outside legend
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

// The value of a series that has none at the crosshair's x.
constexpr QStringView kNoValue = u"—";

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
        return kNoValue.toString();
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

// A square of the bars' fill (and outline).
void drawBarSwatch(QPainter& painter, const BarSeries& bars, QPointF center)
{
    QColor fill = bars.color();
    fill.setAlphaF(static_cast<float>(static_cast<double>(fill.alphaF()) * bars.fillOpacity()));
    const QRectF patch(center - QPointF(kSwatchBar / 2.0, kSwatchBar / 2.0),
                       QSizeF(kSwatchBar, kSwatchBar));
    painter.fillRect(patch, fill);
    if (bars.isOutlineVisible())
    {
        painter.setPen(QPen(bars.color(), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(patch.adjusted(0.5, 0.5, -0.5, -0.5));
    }
}

// A line (for line series) with the series' marker across the middle, over its errors; a patch
// of bars, under them.
void drawSwatch(QPainter& painter, const Series& series, const Theme& theme, QPointF center)
{
    switch (series.kind())
    {
        case SeriesKind::LINE:
            drawErrorSwatch(painter, series, theme, center);
            if (const auto* line = qobject_cast<const LineSeries*>(&series))
            {
                painter.setPen(linePen(*line, std::min(line->lineWidth(), kMaxSwatchLine)));
                painter.drawLine(center - QPointF(kSwatch / 2.0, 0.0),
                                 center + QPointF(kSwatch / 2.0, 0.0));
            }
            MarkerPainter::draw(painter, center, markerStyle(series, theme));
            break;
        case SeriesKind::SCATTER:
            drawErrorSwatch(painter, series, theme, center);
            MarkerPainter::draw(painter, center, markerStyle(series, theme));
            break;
        case SeriesKind::BARS:
            if (const auto* bars = qobject_cast<const BarSeries*>(&series))
            {
                drawBarSwatch(painter, *bars, center);
            }
            drawErrorSwatch(painter, series, theme, center);
            break;
    }
}

// The value an entry shows at the crosshair (at @p x): of the series of the point the crosshair is
// on, or of the first of its series that has one there. None for an annotation.
QString entryValue(const LegendEntry& entry, const PlotLayout& layout, const LegendState& state,
                   double x)
{
    QString value;
    for (const Series* series : entry.series)
    {
        if (!series->isVisible())
        {
            continue;
        }
        if (series == state.crosshairSeries)
        {
            return valueOf(*series, layout, x, state.crosshairIndex);
        }
        if (value.isEmpty() || value == kNoValue)
        {
            value = valueOf(*series, layout, x, std::nullopt);
        }
    }
    return value;
}

// What an annotation looks like, as small as a swatch.
void drawAnnotationSwatch(QPainter& painter, const Annotation& annotation, const PlotLayout& layout,
                          QPointF center)
{
    const Theme& theme = annotation.plot()->theme();
    const QColor color = annotation.color();
    const QRectF patch(center.x() - (kSwatch / 2.0), center.y() - (kSwatchBand / 2.0), kSwatch,
                       kSwatchBand);
    const auto   faded = [&color](double opacity) {
        QColor fill = color;
        fill.setAlphaF(static_cast<float>(static_cast<double>(fill.alphaF()) * opacity));
        return fill;
    };
    if (const auto* line = qobject_cast<const ReferenceLine*>(&annotation))
    {
        painter.setPen(QPen(color, std::min(line->lineWidth(), kMaxSwatchLine), line->lineStyle(),
                            Qt::FlatCap));
        painter.drawLine(center - QPointF(kSwatch / 2.0, 0.0),
                         center + QPointF(kSwatch / 2.0, 0.0));
    }
    else if (const auto* span = qobject_cast<const ShadedSpan*>(&annotation))
    {
        painter.fillRect(patch, faded(span->opacity()));
    }
    else if (const auto* rect = qobject_cast<const RectAnnotation*>(&annotation))
    {
        painter.fillRect(patch, faded(rect->fillOpacity()));
        if (rect->lineStyle() != Qt::NoPen)
        {
            painter.setPen(QPen(color, std::min(rect->lineWidth(), kMaxSwatchLine),
                                rect->lineStyle(), Qt::FlatCap, Qt::MiterJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(patch);
        }
    }
    else if (const auto* icon = qobject_cast<const IconAnnotation*>(&annotation))
    {
        const double side = std::min(kSwatch, QFontMetricsF(layout.legendFont).height());
        drawIcon(painter, icon->icon(),
                 QRectF(center - QPointF(side / 2.0, side / 2.0), QSizeF(side, side)),
                 layout.devicePixelRatio);
    }
    else if (const auto* event = qobject_cast<const EventMarker*>(&annotation))
    {
        // Its line, with a flag at the top, or its icon.
        const double side = QFontMetricsF(layout.legendFont).height();
        if (!event->icon().isNull())
        {
            drawIcon(painter, event->icon(),
                     QRectF(center - QPointF(side / 2.0, side / 2.0), QSizeF(side, side)),
                     layout.devicePixelRatio);
            return;
        }
        const double left = center.x() - (kSwatch / 4.0);
        painter.setPen(QPen(color, theme.annotationLineWidth, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(left, center.y() - (side / 2.0)),
                         QPointF(left, center.y() + (side / 2.0)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(left, center.y() - (side / 2.0), kSwatch / 2.0, side / 2.0),
                                kRowRadius / 2.0, kRowRadius / 2.0);
        painter.setBrush(Qt::NoBrush);
    }
    else if (const auto* note = qobject_cast<const TextAnnotation*>(&annotation))
    {
        // A framed box of its background, with a line of its text color.
        const double pixel = 1.0 / layout.devicePixelRatio;
        painter.setPen(hairlinePen(theme.legendBorder));
        painter.setBrush(note->backgroundColor());
        painter.drawRoundedRect(
            patch.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0), kRowRadius / 2.0,
            kRowRadius / 2.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(patch.left() + 4.0, center.y()),
                         QPointF(patch.right() - 4.0, center.y()));
    }
}

// The width of the value column an outside legend keeps for the values at the crosshair, so that
// it doesn't change size (and the plot area with it) as the crosshair moves: the widest value of
// the ends of its series' y ranges, and a digit more. 0 while no values are shown.
double reservedValueWidth(const PlotWidget& plot, const PlotLayout& layout,
                          const std::vector<LegendEntry>& entries)
{
    if (!plot.isCrosshairEnabled() || !plot.legend()->areValuesVisible())
    {
        return 0.0;
    }
    const QFontMetricsF metrics(valueFont(layout));
    double              widest = 0.0;
    for (const LegendEntry& entry : entries)
    {
        for (const Series* series : entry.series)
        {
            const Axis&             axis = *series->yAxis();
            const core::AxisMapping mapping(axis.range(), layout.bounds.bottom(),
                                            layout.bounds.top(), scaleOf(axis));
            const Range             bounds = series->yBounds();
            for (const double value : {bounds.min, bounds.max})
            {
                if (std::isfinite(value))
                {
                    widest = std::max(widest,
                                      metrics.horizontalAdvance(valueLabel(axis, mapping, value)));
                }
            }
        }
    }
    return widest > 0.0 ? widest + metrics.horizontalAdvance(QLatin1Char('0')) : 0.0;
}

// Places the rows of an outside legend in the cells of its grid.
void placeOutsideRows(LegendLayout& legend, const OutsideLegend& outside)
{
    const double pitch = outside.rowHeight + kRowGap;
    for (std::size_t i = 0; i < legend.entries.size(); ++i)
    {
        const std::size_t column = outside.grid.columnOf(i);
        double            left   = legend.box.left() + kPadding;
        for (std::size_t before = 0; before < column; ++before)
        {
            left += outside.grid.columnWidths[before] + kColumnGap;
        }
        const double top =
            legend.box.top() + kPadding + (static_cast<double>(outside.grid.rowOf(i)) * pitch);
        legend.entries[i].row = QRectF(left - kPadding, top - (kRowGap / 2.0),
                                       outside.grid.columnWidths[column] + (2.0 * kPadding), pitch);
    }
}

}  // namespace

bool isOutside(LegendAnchor anchor) noexcept
{
    return anchor == LegendAnchor::OUTSIDE_BOTTOM || anchor == LegendAnchor::OUTSIDE_RIGHT;
}

std::vector<LegendEntry> legendEntries(const PlotWidget& plot)
{
    std::vector<LegendEntry> entries;
    for (Series* series : plot.series())
    {
        const QString group = series->legendGroup();
        const auto joined = group.isEmpty()
                                ? entries.end()
                                : std::ranges::find_if(entries, [&group](const LegendEntry& entry) {
                                      return entry.group == group;
                                  });
        if (joined != entries.end())
        {
            joined->series.push_back(series);
            if (joined->sample == nullptr && !series->name().isEmpty())
            {
                joined->sample = series;
                joined->name   = series->name();
            }
        }
        else if (!group.isEmpty() || !series->name().isEmpty())
        {
            LegendEntry entry;
            entry.series = {series};
            entry.group  = group;
            if (!series->name().isEmpty())
            {
                entry.sample = series;
                entry.name   = series->name();
            }
            entries.push_back(std::move(entry));
        }
    }
    std::erase_if(entries, [](const LegendEntry& entry) { return entry.sample == nullptr; });
    for (Annotation* annotation : plot.annotations())
    {
        if (!annotation->legendName().isEmpty())
        {
            LegendEntry entry;
            entry.annotation = annotation;
            entry.name       = annotation->legendName();
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

OutsideLegend measureOutsideLegend(const PlotWidget& plot, const PlotLayout& layout,
                                   TextPainter& text, double available, double limit)
{
    OutsideLegend      result;
    const Legend&      options = *plot.legend();
    const LegendAnchor anchor  = options.anchor();
    if (!isOutside(anchor))
    {
        return result;
    }
    const std::vector<LegendEntry> entries = legendEntries(plot);
    if (!options.isShownFor(static_cast<qsizetype>(entries.size())))
    {
        return result;
    }
    double              nameHeight = QFontMetricsF(layout.legendFont).height();
    std::vector<double> widths;
    result.valueWidth = reservedValueWidth(plot, layout, entries);
    const double fixed =
        kSwatch + kSwatchGap + (result.valueWidth > 0.0 ? kValueGap + result.valueWidth : 0.0);
    for (const LegendEntry& entry : entries)
    {
        const QSizeF name = text.size(entry.name, layout.legendFont);
        nameHeight        = std::max(nameHeight, name.height());
        widths.push_back(fixed + name.width());
    }
    result.rowHeight    = std::max(nameHeight, plot.theme().markerSize + 2.0);
    const double pitch  = result.rowHeight + kRowGap;
    const auto   rowsIn = [pitch](double height) {
        return static_cast<std::size_t>(std::max(1.0, std::floor((height + kRowGap) / pitch)));
    };
    if (anchor == LegendAnchor::OUTSIDE_BOTTOM)
    {
        // Across, in as many columns as fit; rows past the limit are left out.
        result.grid      = core::flowAcross(widths, available - (2.0 * kPadding), kColumnGap);
        result.grid.rows = std::min(result.grid.rows, rowsIn(limit - (2.0 * kPadding)));
        result.entries   = std::min(entries.size(), result.grid.rows * result.grid.columns());
    }
    else
    {
        // Down, in columns as tall as the room; too wide, the names are shortened.
        result.grid          = core::flowDown(widths, rowsIn(available - (2.0 * kPadding)));
        result.entries       = entries.size();
        const auto   columns = static_cast<double>(result.grid.columns());
        const double room    = limit - (2.0 * kPadding) - ((columns - 1.0) * kColumnGap);
        const double names   = result.grid.width(0.0) - (columns * fixed);
        if (result.grid.width(0.0) > room && names > 0.0)
        {
            const double share = std::max(0.0, room - (columns * fixed)) / names;
            for (double& width : result.grid.columnWidths)
            {
                width = fixed + ((width - fixed) * share);
            }
        }
    }
    const auto rows = static_cast<double>(result.grid.rows);
    result.box = QRectF(0.0, 0.0, (2.0 * kPadding) + result.grid.width(kColumnGap),
                        (2.0 * kPadding) + (rows * result.rowHeight) + ((rows - 1.0) * kRowGap));
    return result;
}

QObject* LegendEntry::key() const
{
    if (annotation != nullptr)
    {
        return annotation;
    }
    return sample;
}

bool LegendEntry::isVisible() const
{
    if (annotation != nullptr)
    {
        return annotation->isVisible();
    }
    return std::ranges::any_of(series, [](const Series* one) { return one->isVisible(); });
}

const LegendEntry* LegendLayout::entryOf(const QObject* key) const
{
    if (key == nullptr)
    {
        return nullptr;
    }
    const auto entry =
        std::ranges::find_if(entries, [key](const LegendEntry& one) { return one.key() == key; });
    return entry == entries.end() ? nullptr : &*entry;
}

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
        case LegendAnchor::OUTSIDE_BOTTOM:  // placed by the plot's layout
        case LegendAnchor::OUTSIDE_RIGHT:
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
    legend.entries        = legendEntries(plot);
    const Legend& options = *plot.legend();
    if (isOutside(options.anchor()))
    {
        // Where the layout made room for it, with the value column it kept.
        const OutsideLegend& outside = layout.legend;
        if (outside.box.isNull())
        {
            legend.entries.clear();
            return legend;
        }
        legend.entries.resize(std::min(legend.entries.size(), outside.entries));
        legend.box        = outside.box;
        legend.anchor     = options.anchor();
        legend.rowHeight  = outside.rowHeight;
        legend.valueWidth = outside.valueWidth;
        if (state.crosshairX && outside.valueWidth > 0.0)
        {
            for (LegendEntry& entry : legend.entries)
            {
                entry.value = entryValue(entry, layout, state, *state.crosshairX);
            }
        }
        placeOutsideRows(legend, outside);
        return legend;
    }
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
            entry.value = entryValue(entry, layout, state, *state.crosshairX);
            widest      = std::max(widest, metrics.horizontalAdvance(entry.value));
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
                const LegendLayout& legend, TextPainter& text, const QObject* pointed)
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
    if (!isOutside(legend.anchor))  // outside the plot area, it needs no box to stand out
    {
        painter.setPen(hairlinePen(theme.legendBorder));
        painter.setBrush(theme.legendBackground);
        painter.drawRoundedRect(box.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0),
                                kRadius, kRadius);
    }
    const QFont values = valueFont(layout);
    for (const LegendEntry& entry : legend.entries)
    {
        // The entry's cell: the legend's width inside it, a column of the grid outside.
        const QRectF row(entry.row.left(), entry.row.center().y() - (legend.rowHeight / 2.0),
                         entry.row.width(), legend.rowHeight);
        const double textLeft   = row.left() + kPadding + kSwatch + kSwatchGap;
        const double valueRight = row.right() - kPadding;
        const double nameRight =
            legend.valueWidth > 0.0 ? valueRight - legend.valueWidth - kValueGap : valueRight;
        if (pointed != nullptr && entry.key() == pointed)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.legendBorder);
            painter.drawRoundedRect(entry.row.adjusted(kPadding / 2.0, 0.0, -kPadding / 2.0, 0.0),
                                    kRowRadius, kRowRadius);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setOpacity(entry.isVisible() ? 1.0 : kHiddenOpacity);
        const QPointF swatch(row.left() + kPadding + (kSwatch / 2.0), row.center().y());
        if (entry.annotation != nullptr)
        {
            drawAnnotationSwatch(painter, *entry.annotation, layout, swatch);
        }
        else
        {
            drawSwatch(painter, *entry.sample, theme, swatch);
        }
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
