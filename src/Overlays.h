#pragma once

#include <QPointF>
#include <QRectF>
#include <cstddef>
#include <optional>

class QPainter;

namespace rocketplot
{

class PlotWidget;
class Series;
struct PlotLayout;
struct Theme;

// What a plot draws over its cached rendering while the user interacts with it.

/// The data point a crosshair is on, when it follows the data (CrosshairMode::SNAP and TRACE).
struct CrosshairPoint
{
    Series*     series = nullptr;
    std::size_t index  = 0;
    QPointF     position;  ///< Where the point is drawn, in widget coordinates
};

/// Draws the crosshair: lines through @p pointer (widget coordinates) with its coordinates in tags
/// on the axes, or, for a crosshair in a linked plot, a vertical line at @p linkedX (data) with an
/// x tag.
///
/// On a data point (@p point), the lines go through the point instead, under a marker in its
/// color, and the tags give the point's own x and y: the y on the axis its series is drawn
/// against, and none on the other one.
void drawCrosshair(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                   std::optional<QPointF> pointer, std::optional<double> linkedX,
                   const CrosshairPoint* point = nullptr);

/// Draws the box being dragged out to zoom (widget coordinates).
void drawZoomBox(QPainter& painter, const PlotLayout& layout, const Theme& theme,
                 const QRectF& box);

}  // namespace rocketplot
