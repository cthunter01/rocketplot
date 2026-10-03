#pragma once

#include <cstddef>
#include <vector>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/LineBand.h"

namespace rocketplot::core
{

class ErrorData;
class SeriesData;

/// One point's error bars in pixels: a whisker from left to right through the point, and one from
/// top to bottom. Without an error along an axis, both ends there are the point's own position.
struct ErrorBar
{
    PixelPoint center;
    double     left   = 0.0;
    double     right  = 0.0;
    double     top    = 0.0;
    double     bottom = 0.0;
};

/// Fills @p out with the error bars of @p data that reach into @p box, their ends cut off at its
/// edges, with at most one bar per @p cellSize × @p cellSize cell (by the point's position). Only
/// the x errors when @p withY is false (the y errors are then shown as a band). An end a log axis
/// can't show (<= 0) is at the edge of the box. Sorted series only visit the points that can reach
/// the box. Returns the number of points visited.
std::size_t collectErrorBars(const SeriesData& data, const ErrorData& errors, const AxisMapping& x,
                             const AxisMapping& y, PixelBox box, double cellSize, bool withY,
                             std::vector<ErrorBar>& out);

/// Fills @p out with the outline of the band between the low and high ends of the y errors of
/// @p data, which must be sorted by x: closed polygons of a few of @p grid's pixel columns each
/// (see outlineColumns()), one stretch of them per stretch of points without a gap. In each column
/// the band covers what its edges, straight between the points, cover there; like decimateLine(),
/// it only visits the visible index range, and where a column has more than a few points it takes
/// the column's highest and lowest end from the pyramids. The band is cut off at @p clipTop and
/// @p clipBottom (pixels, just outside the plot), which keeps the coordinates small enough to draw
/// exactly. Returns the number of points visited.
std::size_t decimateErrorBand(const SeriesData& data, const ErrorData& errors, const AxisMapping& x,
                              const AxisMapping& y, const ColumnGrid& grid, double clipTop,
                              double clipBottom, Polyline& out);

}  // namespace rocketplot::core
