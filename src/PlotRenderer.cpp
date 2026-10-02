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

// The center of the device pixel that contains @p value (in logical coordinates): a 1-device-pixel
// antialiased line there covers exactly one pixel row or column, so it is crisp.
double crisp(double value, double devicePixelRatio)
{
    return (std::floor(value * devicePixelRatio) + 0.5) / devicePixelRatio;
}

QPen hairline(const QColor& color)
{
    QPen pen(color, 1.0);
    pen.setCosmetic(true);  // 1 device pixel at any scale
    pen.setCapStyle(Qt::FlatCap);
    return pen;
}

core::PixelBox expanded(const QRectF& rect, double margin)
{
    return {.left   = rect.left() - margin,
            .top    = rect.top() - margin,
            .right  = rect.right() + margin,
            .bottom = rect.bottom() + margin};
}

MarkerStyle markerStyle(const Series& series, const Theme& theme)
{
    return {.shape     = series.marker(),
            .size      = series.markerSize(),
            .color     = series.color(),
            .ring      = theme.background,
            .ringWidth = theme.markerRingWidth};
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

PlotRenderer::PlotRenderer(const PlotWidget& plot, const PlotLayout& layout, MarkerPainter& markers)
  : m_plot(&plot), m_layout(&layout), m_theme(&plot.theme()), m_markers(&markers)
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
    const QRectF& plot = m_layout->plot;
    const double  dpr  = m_layout->devicePixelRatio;
    painter.setPen(hairline(m_theme->gridLine));
    if (m_plot->xAxis()->isGridVisible())
    {
        for (const double value : m_layout->x.ticks.major)
        {
            const double px = crisp(m_layout->x.mapping.toPixel(value), dpr);
            if (px >= plot.left() && px <= plot.right())
            {
                painter.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
            }
        }
    }
    if (m_plot->yAxis()->isGridVisible())
    {
        for (const double value : m_layout->y.ticks.major)
        {
            const double py = crisp(m_layout->y.mapping.toPixel(value), dpr);
            if (py >= plot.top() && py <= plot.bottom())
            {
                painter.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
            }
        }
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
    const core::DecimationResult result      = core::decimateLine(
        series.data(), m_layout->x.mapping, m_layout->y.mapping, columnWidth, m_line);
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
        core::decimateScatter(series.data(), m_layout->x.mapping, m_layout->y.mapping,
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
                                          (2.0 * padding))};
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
        core::decimateScatter(series.data(), m_layout->x.mapping, m_layout->y.mapping,
                              expanded(m_layout->plot, style.size), cell, m_points);
    stats.drawnPoints = m_points.size();
    m_markers->draw(painter, m_points, style);
}

void PlotRenderer::drawAxes(QPainter& painter) const
{
    const QRectF& plot  = m_layout->plot;
    const double  dpr   = m_layout->devicePixelRatio;
    const double  pixel = 1.0 / dpr;
    // The axis lines run along the first pixel row below and the first pixel column left of the
    // plot.
    const double axisY = crisp(plot.bottom(), dpr);
    const double axisX = crisp(plot.left() - pixel, dpr);

    painter.setPen(hairline(m_theme->axisLine));
    painter.drawLine(QPointF(axisX - (0.5 * pixel), axisY), QPointF(plot.right(), axisY));
    painter.drawLine(QPointF(axisX, plot.top()), QPointF(axisX, axisY));

    const auto xTick = [&](double value, double length) {
        const double px = crisp(m_layout->x.mapping.toPixel(value), dpr);
        if (px >= plot.left() - pixel && px <= plot.right() + pixel)
        {
            painter.drawLine(QPointF(px, axisY), QPointF(px, axisY + length));
        }
    };
    const auto yTick = [&](double value, double length) {
        const double py = crisp(m_layout->y.mapping.toPixel(value), dpr);
        if (py >= plot.top() - pixel && py <= plot.bottom() + pixel)
        {
            painter.drawLine(QPointF(axisX - length, py), QPointF(axisX, py));
        }
    };
    for (const double value : m_layout->x.ticks.minor)
    {
        xTick(value, m_theme->minorTickLength);
    }
    for (const double value : m_layout->x.ticks.major)
    {
        xTick(value, m_theme->tickLength);
    }
    for (const double value : m_layout->y.ticks.minor)
    {
        yTick(value, m_theme->minorTickLength);
    }
    for (const double value : m_layout->y.ticks.major)
    {
        yTick(value, m_theme->tickLength);
    }
}

void PlotRenderer::drawLabels(QPainter& painter) const
{
    const QRectF& plot = m_layout->plot;

    painter.setFont(m_layout->tickFont);
    painter.setPen(m_theme->secondaryText);
    const double lineHeight = QFontMetricsF(m_layout->tickFont).height();
    const double labelTop   = plot.bottom() + m_theme->tickLength + kTickLabelGap;
    for (qsizetype i = 0; i < m_layout->x.labels.size(); ++i)
    {
        const double px =
            m_layout->x.mapping.toPixel(m_layout->x.ticks.major[static_cast<std::size_t>(i)]);
        painter.drawText(QRectF(px - (kLabelBoxWidth / 2.0), labelTop, kLabelBoxWidth, lineHeight),
                         Qt::AlignHCenter | Qt::AlignTop, m_layout->x.labels.at(i));
    }
    const double labelRight = plot.left() - m_theme->tickLength - kTickLabelGap;
    for (qsizetype i = 0; i < m_layout->y.labels.size(); ++i)
    {
        const double py =
            m_layout->y.mapping.toPixel(m_layout->y.ticks.major[static_cast<std::size_t>(i)]);
        painter.drawText(QRectF(labelRight - kLabelBoxWidth, py - (lineHeight / 2.0),
                                kLabelBoxWidth, lineHeight),
                         Qt::AlignRight | Qt::AlignVCenter, m_layout->y.labels.at(i));
    }

    painter.setFont(m_layout->labelFont);
    if (!m_layout->xLabel.isNull())
    {
        painter.drawText(m_layout->xLabel, Qt::AlignCenter, m_plot->xAxis()->label());
    }
    if (!m_layout->yLabel.isNull())
    {
        // Rotated to read bottom to top.
        painter.save();
        painter.translate(m_layout->yLabel.center());
        painter.rotate(-90.0);
        const QRectF rotated(-m_layout->yLabel.height() / 2.0, -m_layout->yLabel.width() / 2.0,
                             m_layout->yLabel.height(), m_layout->yLabel.width());
        painter.drawText(rotated, Qt::AlignCenter, m_plot->yAxis()->label());
        painter.restore();
    }
    if (!m_layout->title.isNull())
    {
        painter.setFont(m_layout->titleFont);
        painter.setPen(m_theme->text);
        painter.drawText(m_layout->title, Qt::AlignCenter, m_plot->title());
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
    painter.setPen(hairline(m_theme->legendBorder));
    painter.setBrush(m_theme->legendBackground);
    painter.drawRoundedRect(box.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0),
                            kLegendRadius, kLegendRadius);
    painter.setBrush(Qt::NoBrush);

    painter.setFont(m_layout->legendFont);
    const QFontMetricsF metrics(m_layout->legendFont);
    const double        textLeft  = box.left() + kLegendPadding + kLegendSwatch + kLegendSwatchGap;
    const double        textWidth = std::max(0.0, box.right() - kLegendPadding - textLeft);
    double              top       = box.top() + kLegendPadding;
    for (const LegendEntry& entry : m_layout->legendEntries)
    {
        painter.setOpacity(entry.series->isVisible() ? 1.0 : kHiddenOpacity);
        const double rowCenter = top + (m_layout->legendRowHeight / 2.0);
        drawLegendSwatch(painter, *entry.series,
                         QPointF(box.left() + kLegendPadding + (kLegendSwatch / 2.0), rowCenter));
        painter.setPen(m_theme->text);
        painter.drawText(QRectF(textLeft, top, textWidth, m_layout->legendRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(entry.name, Qt::ElideRight, textWidth));
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
            painter.setPen(hairline(color));
            painter.drawRect(rect);
        }
    };
    const QColor magenta(255, 0, 255, 200);
    const QColor cyan(0, 200, 255, 200);
    const QColor orange(255, 150, 0, 200);
    outline(layout.plot, magenta);
    outline(layout.xAxisArea, cyan);
    outline(layout.yAxisArea, cyan);
    outline(layout.title, orange);
    outline(layout.xLabel, orange);
    outline(layout.yLabel, orange);
    outline(layout.legend, orange);

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
