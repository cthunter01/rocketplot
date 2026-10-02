#pragma once

#include <QString>
#include <cstddef>
#include <vector>

#include "core/Decimator.h"
#include "core/LineBand.h"

class QPainter;
class QPointF;

namespace rocketplot
{

class LineSeries;
class MarkerPainter;
class PlotWidget;
class ScatterSeries;
class Series;
struct PlotLayout;
struct Theme;

/// What drawing one series took, for the debug overlay.
struct SeriesStats
{
    QString              name;
    bool                 scatter       = false;
    core::DecimationMode mode          = core::DecimationMode::RAW;
    std::size_t          totalPoints   = 0;
    std::size_t          visiblePoints = 0;      ///< Points decimation looked at
    std::size_t          drawnPoints   = 0;      ///< Vertices or markers handed to QPainter
    bool                 band          = false;  ///< A line drawn as a filled outline (dense data)
};

struct RenderStats
{
    double                   milliseconds = 0.0;
    std::vector<SeriesStats> series;
};

/// Paints a plot with a precomputed layout: background, grid, series, axes, labels, legend. It only
/// reads the plot, so the same code can paint the widget or (later) an image or vector export.
class PlotRenderer
{
public:
    PlotRenderer(const PlotWidget& plot, const PlotLayout& layout, MarkerPainter& markers);

    void render(QPainter& painter, RenderStats& stats);

private:
    void drawGrid(QPainter& painter) const;
    void drawSeries(QPainter& painter, RenderStats& stats);
    void drawLine(QPainter& painter, const LineSeries& series, SeriesStats& stats);
    void fillBand(QPainter& painter, const LineSeries& series, double columnWidth);
    void strokeLine(QPainter& painter, const LineSeries& series) const;
    void drawScatter(QPainter& painter, const Series& series, SeriesStats& stats);
    void drawAxes(QPainter& painter) const;
    void drawLabels(QPainter& painter) const;
    void drawLegend(QPainter& painter);
    void drawLegendSwatch(QPainter& painter, const Series& series, QPointF center);

    const PlotWidget*             m_plot;
    const PlotLayout*             m_layout;
    const Theme*                  m_theme;
    MarkerPainter*                m_markers;
    core::Polyline                m_line;  // reused buffers
    core::Polyline                m_clipped;
    core::Polyline                m_band;
    core::LineBand                m_lineBand;
    std::vector<core::PixelPoint> m_points;
};

/// Draws the debug overlay: layout boxes, frame time and what each series drew.
void drawDebugOverlay(QPainter& painter, const PlotLayout& layout, const RenderStats& stats);

}  // namespace rocketplot
