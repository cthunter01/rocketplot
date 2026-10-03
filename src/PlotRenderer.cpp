#include "PlotRenderer.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLineF>
#include <QList>
#include <QLocale>
#include <QPaintEngine>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "AnnotationPainter.h"
#include "MarkerPainter.h"
#include "PlotLayout.h"
#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/ErrorData.h"
#include "core/ErrorGeometry.h"
#include "core/LineBand.h"
#include "core/Occupancy.h"
#include "core/PolylineClipper.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
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
constexpr double      kMinDotRadius   = 2.0;
// While one series is highlighted, the others are drawn this faint.
constexpr double kFadedOpacity = 0.25;
// Labels are centered on their tick in a box this wide (wider than any label).
constexpr double kLabelBoxWidth = 400.0;
// Error bars closer together than this can't be told apart: a series sorted by x then shows its
// y errors as a band. Others draw one bar per cell of at least kMinBarCell pixels.
constexpr double kMinBarSpacing = 4.0;
constexpr double kMinBarCell    = 3.0;

core::PixelBox expanded(const QRectF& rect, double margin)
{
    return {
        .left   = rect.left() - margin,
        .top    = rect.top() - margin,
        .right  = rect.right() + margin,
        .bottom = rect.bottom() + margin,
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

// What was drawn of a series' errors, for the debug overlay; empty for none.
QString errorsName(const SeriesStats& stats)
{
    QString name;
    if (stats.errorBand)
    {
        name += QStringLiteral(", error band");
    }
    if (stats.errorBars > 0)
    {
        name += QStringLiteral(", %1 error bars").arg(QLocale().toString(stats.errorBars));
    }
    return name;
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

void PlotRenderer::render(QPainter& painter, RenderStats& stats, const RenderOptions& options)
{
    m_options = options;
    if (m_options.occupancy != nullptr)
    {
        m_options.occupancy->reset(expanded(m_layout->plot, 0.0));
    }
    painter.fillRect(m_layout->bounds, m_theme->background);
    if (!m_layout->valid)
    {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawGrid(painter);
    painter.save();
    // Within whatever clip the painter came with: PlotWidget::paint() draws into others' drawings.
    painter.setClipRect(m_layout->plot, Qt::IntersectClip);
    drawAnnotations(painter, *m_plot, *m_layout, *m_text, AnnotationLayer::BELOW_SERIES,
                    m_options.occupancy);
    drawSeries(painter, stats);
    drawAnnotations(painter, *m_plot, *m_layout, *m_text, AnnotationLayer::ABOVE_SERIES,
                    m_options.occupancy);
    drawAnnotationLabels(painter, *m_plot, *m_layout, *m_text, m_options.occupancy);
    painter.restore();
    drawAxes(painter);
    drawLabels(painter);
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
    const QList<Series*> all         = m_plot->series();
    const Series*        highlighted = m_options.highlighted;
    if (highlighted != nullptr && !all.contains(highlighted))
    {
        highlighted = nullptr;
    }
    const auto draw = [&](const Series& series) {
        SeriesStats seriesStats{.name = series.name(), .totalPoints = series.size()};
        const auto* line    = qobject_cast<const LineSeries*>(&series);
        seriesStats.scatter = line == nullptr;
        if (series.isVisible())
        {
            painter.setOpacity(highlighted != nullptr && &series != highlighted ? kFadedOpacity
                                                                                : 1.0);
            if (line != nullptr)
            {
                drawLine(painter, *line, seriesStats);
            }
            else
            {
                drawScatter(painter, series, seriesStats);
            }
        }
        stats.series.push_back(seriesStats);
    };
    for (const Series* series : all)
    {
        if (series != highlighted)
        {
            draw(*series);
        }
    }
    // The highlighted series on top of the others.
    if (highlighted != nullptr)
    {
        draw(*highlighted);
    }
    painter.restore();
}

void PlotRenderer::drawLine(QPainter& painter, const LineSeries& series, SeriesStats& stats)
{
    const double             dpr         = m_layout->devicePixelRatio;
    const double             columnWidth = 1.0 / dpr;
    const core::AxisMapping& y           = m_layout->yFor(series).mapping;
    // The errors: a band under the line, bars over it.
    const ErrorPlan errors = errorPlan(series);
    if (errors.band)
    {
        drawErrorBand(painter, series, stats);
    }
    const core::DecimationResult result =
        core::decimateLine(series.data(), m_layout->x.mapping, y, columnWidth, m_line);
    stats.mode          = result.mode;
    stats.visiblePoints = result.visiblePoints;
    // In a drawing (SVG, PDF) lines are cut as close to the plot as their width allows: a viewer
    // that ignores clip paths, as QtSvg's does, then shows them running on a few pixels past the
    // axes rather than many.
    const QPaintEngine* engine = painter.paintEngine();
    const bool          raster = engine == nullptr || engine->type() == QPaintEngine::Raster;
    core::clipPolyline(
        m_line, expanded(m_layout->plot, (raster ? kLineClipMargin : 0.0) + series.lineWidth()),
        m_clipped);
    if (m_options.occupancy != nullptr)
    {
        m_options.occupancy->addPolyline(m_clipped, series.lineWidth());
    }

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
    if (errors.bars)
    {
        drawErrorBars(painter, series, errors.barsWithY, stats);
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
        if (m_options.occupancy != nullptr)
        {
            m_options.occupancy->addPoints(m_points, style.size);
        }
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

    fillPolygons(painter, m_band, series.color());
}

void PlotRenderer::fillPolygons(QPainter& painter, const core::Polyline& polygons,
                                const QColor& color)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    for (std::size_t r = 0; r < polygons.runCount(); ++r)
    {
        m_vertices.clear();
        for (const core::PixelPoint& point : polygons.run(r))
        {
            m_vertices.emplace_back(point.x, point.y);
        }
        painter.drawPolygon(m_vertices.data(), static_cast<int>(m_vertices.size()));
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
    const ErrorPlan errors = errorPlan(series);
    if (errors.band)
    {
        drawErrorBand(painter, series, stats);
    }
    if (errors.bars)
    {
        drawErrorBars(painter, series, errors.barsWithY, stats);
    }

    const MarkerStyle style   = markerStyle(series, *m_theme);
    const auto*       scatter = qobject_cast<const ScatterSeries*>(&series);
    if (scatter != nullptr && (scatter->hasSizes() || scatter->hasColors()))
    {
        drawStyledScatter(painter, *scatter, style, stats);
        return;
    }
    // Markers less than a quarter of their size apart look the same as one: draw one per such cell.
    const double cell = std::max(1.0 / m_layout->devicePixelRatio, style.size / 4.0);
    stats.visiblePoints =
        core::decimateScatter(series.data(), m_layout->x.mapping, m_layout->yFor(series).mapping,
                              expanded(m_layout->plot, style.size), cell, m_points);
    stats.drawnPoints = m_points.size();
    m_markers->draw(painter, m_points, style);
    if (m_options.occupancy != nullptr)
    {
        m_options.occupancy->addPoints(m_points, style.size);
    }
}

void PlotRenderer::drawStyledScatter(QPainter& painter, const ScatterSeries& series,
                                     const MarkerStyle& style, SeriesStats& stats)
{
    const std::span<const double> sizes  = series.sizes();
    const std::span<const QRgb>   colors = series.colors();
    // Markers with their own sizes or colors differ, so only those on the same pixel are drawn as
    // one. The largest reaches into the plot from furthest outside it.
    const double reach = std::max(style.size, series.m_largestSize);
    stats.visiblePoints =
        core::decimateScatter(series.data(), m_layout->x.mapping, m_layout->yFor(series).mapping,
                              expanded(m_layout->plot, reach), 1.0 / m_layout->devicePixelRatio,
                              m_points, &m_indices, sizes);
    stats.drawnPoints = m_points.size();
    // Each drawn point's own size and color; the series' for points added since they were set.
    m_sizes.clear();
    m_colors.clear();
    for (const std::size_t index : m_indices)
    {
        if (!sizes.empty())
        {
            m_sizes.push_back(index < sizes.size() ? sizes[index] : style.size);
        }
        if (!colors.empty())
        {
            m_colors.push_back(index < colors.size() ? colors[index] : style.color.rgba());
        }
    }
    m_markers->draw(painter, m_points, m_sizes, m_colors, style);
    if (m_options.occupancy == nullptr)
    {
        return;
    }
    for (std::size_t i = 0; i < m_points.size(); ++i)
    {
        const double half = (m_sizes.empty() ? style.size : m_sizes[i]) / 2.0;
        m_options.occupancy->addBox({
            .left   = m_points[i].x - half,
            .top    = m_points[i].y - half,
            .right  = m_points[i].x + half,
            .bottom = m_points[i].y + half,
        });
    }
}

PlotRenderer::ErrorPlan PlotRenderer::errorPlan(const Series& series) const
{
    const core::ErrorData& errors = series.errors();
    if (errors.empty())
    {
        return {};
    }
    const bool sorted = series.isSortedByX();
    bool       dense  = false;
    if (sorted)
    {
        const auto [first, last] =
            core::visibleIndexRange(series.data(), m_layout->x.mapping.range());
        dense = static_cast<double>(last - first) * kMinBarSpacing > m_layout->plot.width();
    }
    const bool band = errors.hasY() && sorted && (dense || series.errorStyle() == ErrorStyle::BAND);
    const bool barsWithY = errors.hasY() && !band;
    return {
        .band      = band,
        .bars      = !dense && (errors.hasX() || barsWithY),
        .barsWithY = barsWithY,
    };
}

void PlotRenderer::drawErrorBand(QPainter& painter, const Series& series, SeriesStats& stats)
{
    // The plot's pixel columns and one either side, so the band runs to the plot's edges.
    const QRectF&          plot        = m_layout->plot;
    const double           columnWidth = 1.0 / m_layout->devicePixelRatio;
    const core::ColumnGrid grid{
        .left  = plot.left() - columnWidth,
        .width = columnWidth,
        .count = static_cast<std::size_t>(std::ceil(plot.width() / columnWidth)) + 2,
    };
    core::decimateErrorBand(series.data(), series.errors(), m_layout->x.mapping,
                            m_layout->yFor(series).mapping, grid, plot.top() - kLineClipMargin,
                            plot.bottom() + kLineClipMargin, m_errorBand);
    QColor fill = series.color();
    fill.setAlphaF(static_cast<float>(static_cast<double>(fill.alphaF()) * series.bandOpacity()));
    fillPolygons(painter, m_errorBand, fill);
    stats.errorBand = true;
}

void PlotRenderer::drawErrorBars(QPainter& painter, const Series& series, bool withY,
                                 SeriesStats& stats)
{
    const double cap   = series.errorCapSize();
    const double width = m_theme->errorBarWidth;
    // Bars are cut off outside the plot, far enough out that the caps drawn there don't show. One
    // per cell the size of a cap: closer together, bars cover each other's.
    core::collectErrorBars(series.data(), series.errors(), m_layout->x.mapping,
                           m_layout->yFor(series).mapping, expanded(m_layout->plot, cap + width),
                           std::max(kMinBarCell, cap), withY, m_errorBars);
    m_lines.clear();
    const double half = cap / 2.0;
    for (const core::ErrorBar& bar : m_errorBars)
    {
        const double x = bar.center.x;
        const double y = bar.center.y;
        if (bar.top < bar.bottom)
        {
            m_lines.emplace_back(x, bar.top, x, bar.bottom);
            // A cap at each end that isn't the point itself (no error on that side).
            for (const double end : {bar.top, bar.bottom})
            {
                if (cap > 0.0 && end != y)
                {
                    m_lines.emplace_back(x - half, end, x + half, end);
                }
            }
        }
        if (bar.left < bar.right)
        {
            m_lines.emplace_back(bar.left, y, bar.right, y);
            for (const double end : {bar.left, bar.right})
            {
                if (cap > 0.0 && end != x)
                {
                    m_lines.emplace_back(end, y - half, end, y + half);
                }
            }
        }
        if (m_options.occupancy != nullptr)
        {
            m_options.occupancy->addBox({
                .left   = std::min(bar.left, x - half),
                .top    = std::min(bar.top, y - half),
                .right  = std::max(bar.right, x + half),
                .bottom = std::max(bar.bottom, y + half),
            });
        }
    }
    painter.setPen(QPen(series.color(), width, Qt::SolidLine, Qt::FlatCap));
    painter.drawLines(m_lines.data(), static_cast<int>(m_lines.size()));
    stats.errorBars = m_errorBars.size();
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

void drawDebugOverlay(QPainter& painter, const PlotLayout& layout, const RenderStats& stats,
                      const QRectF& legend)
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
    outline(legend, orange);
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
        lines << QStringLiteral("%1: %2 pts, %3 visible → %4 drawn (%5%6)")
                     .arg(name, locale.toString(series.totalPoints),
                          locale.toString(series.visiblePoints),
                          locale.toString(series.drawnPoints), modeName(series),
                          errorsName(series));
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
