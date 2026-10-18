#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/NearestPoint.h"

namespace rocketplot::core
{

/// A bar as drawn: its rectangle in pixels, and the first of the bars it stands for (bars
/// narrower than a device pixel are merged).
struct BarRect
{
    PixelBox    box;
    std::size_t index = 0;
};

/// How bars are drawn: the share of each bar's width left empty between neighbours (0: they touch),
/// and the device pixels per pixel that edges are snapped to.
struct BarStyle
{
    double gap              = 0.0;
    double devicePixelRatio = 1.0;
};

/// The bin of the bars between @p edges (n + 1 increasing values for n bars) that holds @p x: from
/// its left edge up to the next, the last bin with its right edge too. Nothing outside them.
[[nodiscard]] std::optional<std::size_t> binAt(std::span<const double> edges, double x);

/// The rectangles of the bars between @p edges with @p heights, seen through the @p x and @p y
/// mappings, for those in @p clip (the plot area). Each stands on 0, or on the bottom of @p clip on
/// a log y axis. Edges are snapped to device pixels, and neighbours that touch share theirs, so
/// that no seam or overlap shows between them. Bars narrower than a device pixel merge with their
/// neighbours into one at least that wide, as tall as the tallest of them. A bar with a NaN
/// height, or one a log axis can't show, is left out.
void barRects(std::span<const double> edges, std::span<const double> heights, const AxisMapping& x,
              const AxisMapping& y, PixelBox clip, BarStyle style, std::vector<BarRect>& out);

/// The bar a crosshair asked by @p query lands on: tracing, the bar whose bin holds the pointer's
/// x; snapping, the bar nearest the pointer within the reach (0 inside it). Its pixel is the middle
/// of its top, and its distance from the pointer is to its rectangle.
[[nodiscard]] std::optional<NearestPoint> pickBar(std::span<const double> edges,
                                                  std::span<const double> heights,
                                                  const PickQuery&        query);

}  // namespace rocketplot::core
