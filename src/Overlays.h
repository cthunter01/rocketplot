#pragma once

#include <QPointF>
#include <QRectF>
#include <optional>

class QPainter;

namespace rocketplot
{

class PlotWidget;
struct PlotLayout;
struct Theme;

// What a plot draws over its cached rendering while the user interacts with it.

/// Draws the crosshair: lines through @p pointer (widget coordinates) with its coordinates in tags
/// on the axes, or, for a crosshair in a linked plot, a vertical line at @p linkedX (data) with an
/// x tag.
void drawCrosshair(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                   std::optional<QPointF> pointer, std::optional<double> linkedX);

/// Draws the box being dragged out to zoom (widget coordinates).
void drawZoomBox(QPainter& painter, const PlotLayout& layout, const Theme& theme,
                 const QRectF& box);

}  // namespace rocketplot
