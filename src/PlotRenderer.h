#pragma once

#include <QLineF>
#include <QPointF>
#include <QRgb>
#include <QString>
#include <cstddef>
#include <vector>

#include "core/Decimator.h"
#include "core/ErrorGeometry.h"
#include "core/LineBand.h"
#include "core/Occupancy.h"

class QColor;
class QPainter;
class QPen;
class QRectF;

namespace rocketplot
{

class LineSeries;
class MarkerPainter;
struct MarkerStyle;
class PlotWidget;
class ScatterSeries;
class Series;
class TextPainter;
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
    std::size_t          errorBars     = 0;      ///< Error bars drawn
    bool                 errorBand     = false;  ///< y errors drawn as a band
};

struct RenderStats
{
    double                   milliseconds = 0.0;
    std::vector<SeriesStats> series;
};

/// How to draw a frame, beyond what the plot says.
struct RenderOptions
{
    /// Drawn on top of the others, which are faded (the series whose legend entry the user points
    /// at).
    const Series* highlighted = nullptr;
    /// Records where data is drawn, for placing the legend.
    core::Occupancy* occupancy = nullptr;
};

/// Paints a plot with a precomputed layout: background, grid, annotations, series, axes and labels
/// (the legend is drawn over it, see LegendLayout.h). It only reads the plot, so the same code can
/// paint the widget or (later) an image or vector export.
class PlotRenderer
{
public:
    PlotRenderer(const PlotWidget& plot, const PlotLayout& layout, MarkerPainter& markers,
                 TextPainter& text);

    void render(QPainter& painter, RenderStats& stats, const RenderOptions& options = {});

private:
    void drawGrid(QPainter& painter) const;
    void drawSeries(QPainter& painter, RenderStats& stats);
    void drawLine(QPainter& painter, const LineSeries& series, SeriesStats& stats);
    void fillBand(QPainter& painter, const LineSeries& series, double columnWidth);
    // Fills each run of @p polygons in @p color.
    void fillPolygons(QPainter& painter, const core::Polyline& polygons, const QColor& color);
    void strokeLine(QPainter& painter, const LineSeries& series) const;
    void drawScatter(QPainter& painter, const Series& series, SeriesStats& stats);
    void drawStyledScatter(QPainter& painter, const ScatterSeries& series, const MarkerStyle& style,
                           SeriesStats& stats);
    // How a series' errors are drawn in this frame.
    struct ErrorPlan
    {
        bool band      = false;  // the y errors as a band
        bool bars      = false;  // bars: of the x errors, and of the y errors if barsWithY
        bool barsWithY = false;
    };
    [[nodiscard]] ErrorPlan errorPlan(const Series& series) const;
    void drawErrorBand(QPainter& painter, const Series& series, SeriesStats& stats);
    void drawErrorBars(QPainter& painter, const Series& series, bool withY, SeriesStats& stats);
    void drawAxes(QPainter& painter) const;
    void drawLabels(QPainter& painter) const;

    const PlotWidget*             m_plot;
    const PlotLayout*             m_layout;
    const Theme*                  m_theme;
    MarkerPainter*                m_markers;
    TextPainter*                  m_text;
    RenderOptions                 m_options;
    core::Polyline                m_line;  // reused buffers
    core::Polyline                m_clipped;
    core::Polyline                m_band;
    core::LineBand                m_lineBand;
    std::vector<core::PixelPoint> m_points;
    std::vector<std::size_t>      m_indices;  // of m_points, for markers with their own style
    std::vector<double>           m_sizes;
    std::vector<QRgb>             m_colors;
    core::Polyline                m_errorBand;
    std::vector<QPointF>          m_vertices;
    std::vector<core::ErrorBar>   m_errorBars;
    std::vector<QLineF>           m_lines;
};

/// The center of the device pixel that contains @p value (in logical coordinates): a 1-device-pixel
/// antialiased line there covers exactly one pixel row or column, so it is crisp.
[[nodiscard]] double crispPixel(double value, double devicePixelRatio);
/// A pen one device pixel wide at any scale, for axes, grid lines and outlines.
[[nodiscard]] QPen hairlinePen(const QColor& color);

/// Draws the debug overlay: layout boxes (and the @p legend box), frame time and what each series
/// drew.
void drawDebugOverlay(QPainter& painter, const PlotLayout& layout, const RenderStats& stats,
                      const QRectF& legend);

}  // namespace rocketplot
