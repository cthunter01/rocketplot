#pragma once

#include "rocketplot/enums.h"

class QPainter;

namespace rocketplot
{

namespace core
{
class Occupancy;
}

class PlotWidget;
class TextPainter;
struct PlotLayout;

// A plot's annotations are part of its rendering (they move with the data), drawn in two passes
// around the series and one after them. The painter must be clipped to the plot area.

/// Draws the annotations of @p plot on @p layer: shaded spans, reference lines, the lines of event
/// markers, and text with its arrow. What a legend should keep clear of is recorded in
/// @p occupancy (if given).
void drawAnnotations(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                     TextPainter& text, AnnotationLayer layer, core::Occupancy* occupancy);

/// Draws the labels of the plot's spans and reference lines and the flags of its event markers,
/// which go over the series whatever the annotation's layer.
void drawAnnotationLabels(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                          TextPainter& text, core::Occupancy* occupancy);

}  // namespace rocketplot
