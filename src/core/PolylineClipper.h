#pragma once

#include <vector>

#include "core/Decimator.h"

namespace rocketplot::core
{

/// Clips the segment a-b to @p box (Liang-Barsky), moving the ends onto the box's edges.
/// @return false if no part of the segment is inside the box (a and b are then unchanged).
bool clipSegment(PixelPoint& a, PixelPoint& b, PixelBox box) noexcept;

/// Writes the parts of @p in that lie inside @p box to @p out, splitting a run where it leaves the
/// box. A single-point run is kept if the point is inside. Used to keep coordinates small enough
/// for QPainter to draw exactly: a point a million pixels off screen becomes the point where its
/// segment crosses the box.
///
/// With @p runOffsets, also records for each run of @p out how far along its run of @p in it
/// starts (in pixels): a dashed line goes on with its pattern where it comes back into the box.
void clipPolyline(const Polyline& in, PixelBox box, Polyline& out,
                  std::vector<double>* runOffsets = nullptr);

}  // namespace rocketplot::core
