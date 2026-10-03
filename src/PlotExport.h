#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <functional>

class QPainter;
class QRectF;

namespace rocketplot
{

// The paint devices a plot is exported to. Each lays a drawing of @p size (device-independent
// pixels) out for a PlotPainter to fill.

/// Paints a plot into @p bounds of @p painter, whose device has @p devicePixelRatio device pixels
/// to each device-independent one.
using PlotPainter =
    std::function<void(QPainter& painter, const QRectF& bounds, double devicePixelRatio)>;

/// An image with @p pixelRatio pixels to each device-independent pixel (and a resolution to
/// match, 96 dpi at 1). Null if it can't be made (no size, or too large).
[[nodiscard]] QImage paintImage(QSize size, double pixelRatio, const PlotPainter& paint);

/// Writes an SVG drawing; false if the file can't be written. @p logicalDpi is the screen's, so
/// that text sized in points is as large in the drawing as it is on screen.
[[nodiscard]] bool writeSvg(const QString& fileName, QSize size, int logicalDpi,
                            const QString& title, const PlotPainter& paint);

/// Writes a PDF document of one page the size of the plot; false if the file can't be written.
[[nodiscard]] bool writePdf(const QString& fileName, QSize size, int logicalDpi,
                            const QString& title, const PlotPainter& paint);

}  // namespace rocketplot
