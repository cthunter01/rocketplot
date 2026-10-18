#pragma once

#include <QRectF>
#include <vector>

#include "rocketplot/enums.h"

class QIcon;
class QPainter;

namespace rocketplot
{

namespace core
{
class Occupancy;
}

class Annotation;
class PlotWidget;
class TextPainter;
struct PlotLayout;

/// Where an annotation drew something the pointer can be on (its box, flag, icon or label), for
/// tool tips and hovering.
struct AnnotationHit
{
    const Annotation* annotation = nullptr;
    QRectF            box;
};

/// What drawing annotations records besides the drawing, where wanted (null: nothing).
struct AnnotationRecord
{
    /// What a legend placed at its BEST spot should keep clear of.
    core::Occupancy* occupancy = nullptr;
    /// What the pointer can be on, in drawing order (the last on top).
    std::vector<AnnotationHit>* hits = nullptr;
};

// A plot's annotations are part of its rendering (they move with the data), drawn in two passes
// around the series and one after them. The painter must be clipped to the plot area.

/// Draws the annotations of @p plot on @p layer: shaded spans, rectangles, reference lines, the
/// lines of event markers, text with its arrow and icons; and records what @p record asks for.
void drawAnnotations(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                     TextPainter& text, AnnotationLayer layer, const AnnotationRecord& record);

/// Draws the labels of the plot's spans, rectangles and reference lines and the flags of its event
/// markers, which go over the series whatever the annotation's layer.
void drawAnnotationLabels(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                          TextPainter& text, const AnnotationRecord& record);

/// Draws the flags of event markers in their band above the plot area, with their lines down to it
/// (FlagPlacement::ABOVE_PLOT_AREA; nothing otherwise). The painter must not be clipped to the plot
/// area.
void drawFlagBand(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                  TextPainter& text, const AnnotationRecord& record);

/// Draws @p icon into @p box (device-independent pixels): sharp on the screen and in images (a
/// pixmap at @p devicePixelRatio), and in drawings (SVG, PDF, print) at four times the size at
/// least, as those keep it as an image.
void drawIcon(QPainter& painter, const QIcon& icon, const QRectF& box, double devicePixelRatio);

}  // namespace rocketplot
