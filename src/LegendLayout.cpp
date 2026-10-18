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
#include "core/Occupancy.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
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

// The legend's entries: one per named series, one per legend group with a named series (named and
// looking after the first named one), then one per annotation with a legend name.
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

}  // namespace

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
        if (pointed != nullptr && entry.key() == pointed)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.legendBorder);
            painter.drawRoundedRect(entry.row.adjusted(kPadding / 2.0, 0.0, -kPadding / 2.0, 0.0),
                                    kRowRadius, kRowRadius);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setOpacity(entry.isVisible() ? 1.0 : kHiddenOpacity);
        const QPointF swatch(box.left() + kPadding + (kSwatch / 2.0), row.center().y());
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
