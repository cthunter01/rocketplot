#include "PlotRenderer.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "MarkerPainter.h"
#include "PlotLayout.h"
#include "TextPainter.h"
#include "core/Decimator.h"
#include "core/LineBand.h"
#include "core/PolylineClipper.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// Lines are clipped this far outside the plot (then by the painter's clip at its edge), so thick
// lines and round caps at the edge are drawn as they would be without clipping.
constexpr double      kLineClipMargin = 16.0;
constexpr std::size_t kPolylineChunk  = 2048;
constexpr double      kHiddenOpacity  = 0.35;
constexpr double      kMinDotRadius   = 2.0;
constexpr double      kLegendRadius   = 4.0;
constexpr double      kMaxSwatchWidth = 3.0;
// Labels are centered on their tick in a box this wide (wider than any label).
constexpr double kLabelBoxWidth = 400.0;

core::PixelBox expanded(const QRectF& rect, double margin)
{
    return {
        .left   = rect.left() - margin,
        .top    = rect.top() - margin,
        .right  = rect.right() + margin,
        .bottom = rect.bottom() + margin,
    };
}

MarkerStyle markerStyle(const Series& series, const Theme& theme)
{
    return {
        .shape     = series.marker(),
        .size      = series.markerSize(),
        .color     = series.color(),
        .ring      = theme.background,
        .ringWidth = theme.markerRingWidth,
    };
}

QString modeName(const SeriesStats& stats)
{
    if (stats.scatter)
    {
        return QStringLiteral("markers");
    }
    QString mode;
    switch (stats.mode)
    {
        case core::DecimationMode::RAW:
            mode = QStringLiteral("raw");
            break;
        case core::DecimationMode::MIN_MAX:
            mode = QStringLiteral("min-max");
            break;
        case core::DecimationMode::PIXEL_SKIP:
            mode = QStringLiteral("pixel-skip");
            break;
    }
    return mode + (stats.band ? QStringLiteral(", band") : QStringLiteral(", stroked"));
}

}  // namespace

double crispPixel(double value, double devicePixelRatio)
{
    return (std::floor(value * devicePixelRatio) + 0.5) / devicePixelRatio;
}

QPen hairlinePen(const QColor& color)
{
    QPen pen(color, 1.0);
    pen.setCosmetic(true);  // 1 device pixel at any scale
    pen.setCapStyle(Qt::FlatCap);
    return pen;
}

PlotRenderer::PlotRenderer(const PlotWidget& plot, const PlotLayout& layout, MarkerPainter& markers,
                           TextPainter& text)
  : m_plot(&plot), m_layout(&layout), m_theme(&plot.theme()), m_markers(&markers), m_text(&text)
{
}

void PlotRenderer::render(QPainter& painter, RenderStats& stats)
{
    painter.fillRect(m_layout->bounds, m_theme->background);
    if (!m_layout->valid)
    {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawGrid(painter);
    drawSeries(painter, stats);
    drawAxes(painter);
    drawLabels(painter);
    drawLegend(painter);
}

void PlotRenderer::drawGrid(QPainter& painter) const
{
    const QRectF& plot     = m_layout->plot;
    const double  dpr      = m_layout->devicePixelRatio;
    const auto    vertical = [&](const std::vector<double>& values, const AxisLayout& axis,
                                 const QColor& color) {
        painter.setPen(hairlinePen(color));
        for (const double value : values)
        {
            const double px = crispPixel(axis.mapping.toPixel(value), dpr);
            if (px >= plot.left() && px <= plot.right())
            {
                painter.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
            }
        }
    };
    const auto horizontal = [&](const std::vector<double>& values, const AxisLayout& axis,
                                const QColor& color) {
        painter.setPen(hairlinePen(color));
        for (const double value : values)
        {
            const double py = crispPixel(axis.mapping.toPixel(value), dpr);
            if (py >= plot.top() && py <= plot.bottom())
            {
                painter.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
            }
        }
    };
    // Minor grid lines first, so the major ones cross over them.
    const Axis& x  = *m_plot->xAxis();
    const Axis& y  = *m_plot->yAxis();
    const Axis& y2 = *m_plot->yAxis2();
    if (x.isMinorGridVisible())
    {
        vertical(m_layout->x.minor, m_layout->x, m_theme->minorGridLine);
    }
    if (y.isMinorGridVisible())
    {
        horizontal(m_layout->y.minor, m_layout->y, m_theme->minorGridLine);
    }
    if (m_layout->y2.shown && y2.isMinorGridVisible())
    {
        horizontal(m_layout->y2.minor, m_layout->y2, m_theme->minorGridLine);
    }
    if (x.isGridVisible())
    {
        vertical(m_layout->x.major, m_layout->x, m_theme->gridLine);
    }
    if (y.isGridVisible())
    {
        horizontal(m_layout->y.major, m_layout->y, m_theme->gridLine);
    }
    if (m_layout->y2.shown && y2.isGridVisible())
    {
        horizontal(m_layout->y2.major, m_layout->y2, m_theme->gridLine);
    }
}

void PlotRenderer::drawSeries(QPainter& painter, RenderStats& stats)
{
    painter.save();
    painter.setClipRect(m_layout->plot);
    for (const Series* series : m_plot->series())
    {
        SeriesStats seriesStats{.name = series->name(), .totalPoints = series->size()};
        const auto* line    = qobject_cast<const LineSeries*>(series);
        seriesStats.scatter = line == nullptr;
        if (series->isVisible())
        {
            if (line != nullptr)
            {
                drawLine(painter, *line, seriesStats);
            }
            else
            {
                drawScatter(painter, *series, seriesStats);
            }
        }
        stats.series.push_back(seriesStats);
    }
    painter.restore();
}

void PlotRenderer::drawLine(QPainter& painter, const LineSeries& series, SeriesStats& stats)
{
    const double                 dpr         = m_layout->devicePixelRatio;
    const double                 columnWidth = 1.0 / dpr;
    const core::AxisMapping&     y           = m_layout->yFor(series).mapping;
    const core::DecimationResult result =
        core::decimateLine(series.data(), m_layout->x.mapping, y, columnWidth, m_line);
    stats.mode          = result.mode;
    stats.visiblePoints = result.visiblePoints;
    core::clipPolyline(m_line, expanded(m_layout->plot, kLineClipMargin + series.lineWidth()),
                       m_clipped);

    // More than a point per pixel column of sorted data: fill the line's outline (fast) instead of
    // stroking it.
    const double columns = m_layout->plot.width() / columnWidth;
    stats.band           = series.isSortedByX() && series.lineStyle() == Qt::SolidLine &&
                           static_cast<double>(result.visiblePoints) > columns;
    if (stats.band)
    {
        fillBand(painter, series, columnWidth);
        stats.drawnPoints = m_band.pointCount();
    }
    else
    {
        strokeLine(painter, series);
        stats.drawnPoints = m_clipped.pointCount();
    }

    // Markers only once the points are apart: closer than half a marker, they would merge into a
    // smear.
    if (series.marker() != Marker::NONE &&
        static_cast<double>(result.visiblePoints) * series.markerSize() / 2.0 <=
            m_layout->plot.width())
    {
        const MarkerStyle style = markerStyle(series, *m_theme);
        core::decimateScatter(series.data(), m_layout->x.mapping, y,
                              expanded(m_layout->plot, style.size), columnWidth, m_points);
        m_markers->draw(painter, m_points, style);
    }
}

void PlotRenderer::fillBand(QPainter& painter, const LineSeries& series, double columnWidth)
{
    const double halfWidth = series.lineWidth() / 2.0;
    const double padding =
        std::ceil(halfWidth / columnWidth) + 1.0;  // columns either side of the plot
    const core::ColumnGrid grid{
        .left  = m_layout->plot.left() - (padding * columnWidth),
        .width = columnWidth,
        .count = static_cast<std::size_t>(std::ceil(m_layout->plot.width() / columnWidth) +
                                          (2.0 * padding)),
    };
    m_lineBand.outline(m_clipped, grid, halfWidth, m_band);

    painter.setPen(Qt::NoPen);
    painter.setBrush(series.color());
    std::vector<QPointF> vertices;
    for (std::size_t r = 0; r < m_band.runCount(); ++r)
    {
        const auto polygon = m_band.run(r);
        vertices.clear();
        for (const core::PixelPoint& point : polygon)
        {
            vertices.emplace_back(point.x, point.y);
        }
        painter.drawPolygon(vertices.data(), static_cast<int>(vertices.size()));
    }
    painter.setBrush(Qt::NoBrush);
}

void PlotRenderer::strokeLine(QPainter& painter, const LineSeries& series) const
{
    painter.setPen(series.pen());
    std::vector<QPointF> vertices;
    for (std::size_t r = 0; r < m_clipped.runCount(); ++r)
    {
        const auto run = m_clipped.run(r);
        if (run.size() == 1)
        {
            // A lone sample between gaps: a dot twice the line width, so it doesn't vanish.
            const double radius = std::max(kMinDotRadius, series.lineWidth());
            painter.setPen(Qt::NoPen);
            painter.setBrush(series.color());
            painter.drawEllipse(QPointF(run.front().x, run.front().y), radius, radius);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(series.pen());
            continue;
        }
        // In short pieces, overlapping by a point: QPainter strokes a long self-overlapping
        // polyline far more slowly (its outline is filled as one polygon). The round caps hide the
        // joins.
        for (std::size_t begin = 0; begin + 1 < run.size(); begin += kPolylineChunk - 1)
        {
            const std::size_t end = std::min(run.size(), begin + kPolylineChunk);
            vertices.clear();
            for (std::size_t i = begin; i < end; ++i)
            {
                vertices.emplace_back(run[i].x, run[i].y);
            }
            painter.drawPolyline(vertices.data(), static_cast<int>(vertices.size()));
        }
    }
}

void PlotRenderer::drawScatter(QPainter& painter, const Series& series, SeriesStats& stats)
{
    const MarkerStyle style = markerStyle(series, *m_theme);
    // Markers less than a quarter of their size apart look the same as one: draw one per such cell.
    const double cell = std::max(1.0 / m_layout->devicePixelRatio, style.size / 4.0);
    stats.visiblePoints =
        core::decimateScatter(series.data(), m_layout->x.mapping, m_layout->yFor(series).mapping,
                              expanded(m_layout->plot, style.size), cell, m_points);
    stats.drawnPoints = m_points.size();
    m_markers->draw(painter, m_points, style);
}

void PlotRenderer::drawAxes(QPainter& painter) const
{
    const QRectF& plot  = m_layout->plot;
    const double  dpr   = m_layout->devicePixelRatio;
    const double  pixel = 1.0 / dpr;
    // The axis lines run along the first pixel row below the plot, the first pixel column left of
    // it and the first one right of it.
    const double axisY  = crispPixel(plot.bottom(), dpr);
    const double axisX  = crispPixel(plot.left() - pixel, dpr);
    const double axisX2 = crispPixel(plot.right(), dpr);
    painter.setPen(hairlinePen(m_theme->axisLine));

    const auto xTicks = [&](const std::vector<double>& values, double length) {
        for (const double value : values)
        {
            const double px = crispPixel(m_layout->x.mapping.toPixel(value), dpr);
            if (px >= plot.left() - pixel && px <= plot.right() + pixel)
            {
                painter.drawLine(QPointF(px, axisY), QPointF(px, axisY + length));
            }
        }
    };
    // Ticks of a y axis whose line is at x, pointing away from the plot (direction -1: left).
    const auto yTicks = [&](const AxisLayout& axis, const std::vector<double>& values, double x,
                            double direction, double length) {
        for (const double value : values)
        {
            const double py = crispPixel(axis.mapping.toPixel(value), dpr);
            if (py >= plot.top() - pixel && py <= plot.bottom() + pixel)
            {
                painter.drawLine(QPointF(x + (direction * length), py), QPointF(x, py));
            }
        }
    };
    if (m_layout->x.shown)
    {
        painter.drawLine(QPointF(axisX - (0.5 * pixel), axisY),
                         QPointF(axisX2 + (0.5 * pixel), axisY));
        xTicks(m_layout->x.minor, m_theme->minorTickLength);
        xTicks(m_layout->x.major, m_theme->tickLength);
    }
    if (m_layout->y.shown)
    {
        painter.drawLine(QPointF(axisX, plot.top()), QPointF(axisX, axisY));
        yTicks(m_layout->y, m_layout->y.minor, axisX, -1.0, m_theme->minorTickLength);
        yTicks(m_layout->y, m_layout->y.major, axisX, -1.0, m_theme->tickLength);
    }
    if (m_layout->y2.shown)
    {
        painter.drawLine(QPointF(axisX2, plot.top()), QPointF(axisX2, axisY));
        yTicks(m_layout->y2, m_layout->y2.minor, axisX2, 1.0, m_theme->minorTickLength);
        yTicks(m_layout->y2, m_layout->y2.major, axisX2, 1.0, m_theme->tickLength);
    }
}

void PlotRenderer::drawLabels(QPainter& painter) const
{
    const QRectF& plot       = m_layout->plot;
    const double  lineHeight = QFontMetricsF(m_layout->tickFont).height();
    painter.setFont(m_layout->tickFont);
    painter.setPen(m_theme->secondaryText);

    if (m_layout->x.shown)
    {
        const double labelTop = plot.bottom() + m_theme->tickLength + kTickLabelGap;
        for (qsizetype i = 0; i < m_layout->x.labels.size(); ++i)
        {
            const double px =
                m_layout->x.mapping.toPixel(m_layout->x.major[static_cast<std::size_t>(i)]);
            painter.drawText(
                QRectF(px - (kLabelBoxWidth / 2.0), labelTop, kLabelBoxWidth, lineHeight),
                Qt::AlignHCenter | Qt::AlignTop, m_layout->x.labels.at(i));
        }
    }
    // y labels right-aligned left of the left axis; y2 labels left-aligned right of the right one.
    const auto yLabels = [&](const AxisLayout& axis, double edge, bool left) {
        for (qsizetype i = 0; i < axis.labels.size(); ++i)
        {
            const double        py = axis.mapping.toPixel(axis.major[static_cast<std::size_t>(i)]);
            const QRectF        box(left ? edge - kLabelBoxWidth : edge, py - (lineHeight / 2.0),
                                    kLabelBoxWidth, lineHeight);
            const Qt::Alignment alignment =
                (left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
            painter.drawText(box, static_cast<int>(alignment.toInt()), axis.labels.at(i));
        }
    };
    if (m_layout->y.shown)
    {
        yLabels(m_layout->y, plot.left() - m_theme->tickLength - kTickLabelGap, true);
    }
    if (m_layout->y2.shown)
    {
        yLabels(m_layout->y2, plot.right() + m_theme->tickLength + kTickLabelGap, false);
    }

    // What the tick labels leave out: below the x axis on the right, above the y axes.
    const auto annotation = [&](const AxisLayout& axis, Qt::Alignment alignment) {
        if (axis.shown && !axis.annotation.isEmpty())
        {
            painter.drawText(axis.annotationRect, static_cast<int>(alignment.toInt()),
                             axis.annotation);
        }
    };
    annotation(m_layout->x, Qt::AlignRight | Qt::AlignVCenter);
    annotation(m_layout->y, Qt::AlignLeft | Qt::AlignBottom);
    annotation(m_layout->y2, Qt::AlignRight | Qt::AlignBottom);

    // Axis titles, the y ones rotated to read bottom to top.
    if (!m_layout->x.titleRect.isNull())
    {
        m_text->draw(painter, m_plot->xAxis()->label(), m_layout->labelFont, m_theme->secondaryText,
                     m_layout->x.titleRect, Qt::AlignCenter);
    }
    const auto rotatedTitle = [&](const QRectF& rect, const QString& text) {
        if (rect.isNull())
        {
            return;
        }
        painter.save();
        painter.translate(rect.center());
        painter.rotate(-90.0);
        const QRectF rotated(-rect.height() / 2.0, -rect.width() / 2.0, rect.height(),
                             rect.width());
        // Plain titles longer than the axis are elided rather than cut off.
        m_text->draw(painter, text, m_layout->labelFont, m_theme->secondaryText, rotated,
                     Qt::AlignCenter, true);
        painter.restore();
    };
    rotatedTitle(m_layout->y.titleRect, m_plot->yAxis()->label());
    rotatedTitle(m_layout->y2.titleRect, m_plot->yAxis2()->label());
    if (!m_layout->title.isNull())
    {
        m_text->draw(painter, m_plot->title(), m_layout->titleFont, m_theme->text, m_layout->title,
                     Qt::AlignCenter);
    }
}

void PlotRenderer::drawLegend(QPainter& painter)
{
    if (m_layout->legendEntries.empty())
    {
        return;
    }
    const QRectF& box   = m_layout->legend;
    const double  pixel = 1.0 / m_layout->devicePixelRatio;
    painter.setPen(hairlinePen(m_theme->legendBorder));
    painter.setBrush(m_theme->legendBackground);
    painter.drawRoundedRect(box.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0),
                            kLegendRadius, kLegendRadius);
    painter.setBrush(Qt::NoBrush);

    const double textLeft  = box.left() + kLegendPadding + kLegendSwatch + kLegendSwatchGap;
    const double textWidth = std::max(0.0, box.right() - kLegendPadding - textLeft);
    double       top       = box.top() + kLegendPadding;
    for (const LegendEntry& entry : m_layout->legendEntries)
    {
        painter.setOpacity(entry.series->isVisible() ? 1.0 : kHiddenOpacity);
        const double rowCenter = top + (m_layout->legendRowHeight / 2.0);
        drawLegendSwatch(painter, *entry.series,
                         QPointF(box.left() + kLegendPadding + (kLegendSwatch / 2.0), rowCenter));
        m_text->draw(painter, entry.name, m_layout->legendFont, m_theme->text,
                     QRectF(textLeft, top, textWidth, m_layout->legendRowHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, true);
        top += m_layout->legendRowHeight + kLegendRowGap;
    }
    painter.setOpacity(1.0);
}

void PlotRenderer::drawLegendSwatch(QPainter& painter, const Series& series, QPointF center)
{
    const MarkerStyle style = markerStyle(series, *m_theme);
    if (const auto* line = qobject_cast<const LineSeries*>(&series))
    {
        QPen pen = line->pen();
        pen.setWidthF(std::min(pen.widthF(), kMaxSwatchWidth));
        painter.setPen(pen);
        painter.drawLine(center - QPointF(kLegendSwatch / 2.0, 0.0),
                         center + QPointF(kLegendSwatch / 2.0, 0.0));
    }
    MarkerPainter::draw(painter, center, style);
}

void drawDebugOverlay(QPainter& painter, const PlotLayout& layout, const RenderStats& stats)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::NoBrush);
    const auto outline = [&](const QRectF& rect, const QColor& color) {
        if (!rect.isNull())
        {
            painter.setPen(hairlinePen(color));
            painter.drawRect(rect);
        }
    };
    const QColor magenta(255, 0, 255, 200);
    const QColor cyan(0, 200, 255, 200);
    const QColor orange(255, 150, 0, 200);
    outline(layout.plot, magenta);
    outline(layout.title, orange);
    outline(layout.legend, orange);
    for (const AxisLayout* axis : {&layout.x, &layout.y, &layout.y2})
    {
        if (axis->shown)
        {
            outline(axis->area, cyan);
            outline(axis->titleRect, orange);
            if (!axis->annotation.isEmpty())
            {
                outline(axis->annotationRect, orange);
            }
        }
    }

    const QLocale locale;
    QStringList   lines;
    lines << QStringLiteral("frame %1 ms   %2×%3 @%4x")
                 .arg(stats.milliseconds, 0, 'f', 2)
                 .arg(layout.bounds.width())
                 .arg(layout.bounds.height())
                 .arg(layout.devicePixelRatio);
    const Range xRange = layout.x.mapping.range();
    const Range yRange = layout.y.mapping.range();
    lines << QStringLiteral("x [%1, %2]   y [%3, %4]")
                 .arg(xRange.min, 0, 'g', 8)
                 .arg(xRange.max, 0, 'g', 8)
                 .arg(yRange.min, 0, 'g', 8)
                 .arg(yRange.max, 0, 'g', 8);
    if (layout.y2.shown)
    {
        const Range y2Range = layout.y2.mapping.range();
        lines.back() += QStringLiteral("   y2 [%1, %2]")
                            .arg(y2Range.min, 0, 'g', 8)
                            .arg(y2Range.max, 0, 'g', 8);
    }
    for (const SeriesStats& series : stats.series)
    {
        const QString name = series.name.isEmpty() ? QStringLiteral("(unnamed)") : series.name;
        if (series.visiblePoints == 0 && series.drawnPoints == 0)
        {
            lines << QStringLiteral("%1: %2 pts, nothing visible")
                         .arg(name, locale.toString(series.totalPoints));
            continue;
        }
        lines << QStringLiteral("%1: %2 pts, %3 visible → %4 drawn (%5)")
                     .arg(name, locale.toString(series.totalPoints),
                          locale.toString(series.visiblePoints),
                          locale.toString(series.drawnPoints), modeName(series));
    }

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSizeF(std::max(7.0, layout.tickFont.pointSizeF()));
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    double              width = 0.0;
    for (const QString& line : lines)
    {
        width = std::max(width, metrics.horizontalAdvance(line));
    }
    constexpr double kPadding = 6.0;
    const QRectF     box(layout.plot.left() + kPadding, layout.plot.top() + kPadding,
                         width + (2.0 * kPadding),
                         (static_cast<double>(lines.size()) * metrics.height()) + (2.0 * kPadding));
    painter.fillRect(box, QColor(0, 0, 0, 180));
    painter.setPen(Qt::white);
    double top = box.top() + kPadding;
    for (const QString& line : lines)
    {
        painter.drawText(QPointF(box.left() + kPadding, top + metrics.ascent()), line);
        top += metrics.height();
    }
    painter.restore();
}

}  // namespace rocketplot
