#pragma once

#include <optional>

#include "core/AxisMapping.h"

namespace rocketplot::core
{

class SeriesData;

/// The y that a line through the points of @p data passes through at @p x, as it is drawn: linear
/// between the two points around @p x in the axes' scales (log10 on a log axis), so on a log axis
/// it follows the straight segment on screen. At a point's own x, that point's y.
///
/// Nothing when it can't tell: @p data isn't sorted by x (a path, not a function of x), @p x is
/// outside its x range or isn't finite, or the line has a gap there (a point around @p x whose y
/// isn't finite, or a value a log axis can't show).
[[nodiscard]] std::optional<double> interpolateY(const SeriesData& data, double x, Scale xScale,
                                                 Scale yScale);

}  // namespace rocketplot::core
