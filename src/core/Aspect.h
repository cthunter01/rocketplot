#pragma once

#include <cstdint>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// Which range fitAspect() changes.
enum class AspectFit : std::uint8_t
{
    WIDEN,     ///< The one that shows less, so that both show at least what they did
    FOLLOW_X,  ///< y: x is given (by a linked plot, by the newest data, by the user)
    FOLLOW_Y,  ///< x: y is given
};

/// The ranges fitAspect() makes of two.
struct AspectRanges
{
    Range x;
    Range y;
};

/// @p x and @p y shown in a plot area of @p width by @p height pixels so that a unit of y takes
/// @p ratio times as many pixels as a unit of x (1: to scale), the range that @p fit names widened
/// or narrowed about its middle. Ranges that can't be shown, sizes that aren't positive or a ratio
/// that isn't are returned as they are.
[[nodiscard]] AspectRanges fitAspect(Range x, Range y, double width, double height, double ratio,
                                     AspectFit fit);

}  // namespace rocketplot::core
