#pragma once

#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// Tick positions for one axis. Major ticks carry labels and grid lines; minor ticks subdivide
/// them.
struct Ticks
{
    std::vector<double> major;
    std::vector<double> minor;
    double              step = 0.0;  ///< Distance between major ticks, 0 when there are none
};

/// The smallest "nice" step (1, 2 or 5 times a power of ten) that is at least @p rawStep.
[[nodiscard]] double niceStep(double rawStep);

/// Nice linear ticks for @p range drawn over @p lengthPx pixels, at least @p minSpacingPx apart.
/// Minor ticks divide each step into 5 (or 4 when the step is 2·10ⁿ), and are left out when they
/// would be closer than
/// @p minMinorSpacingPx. Major ticks that should be zero are exactly zero.
[[nodiscard]] Ticks linearTicks(Range range, double lengthPx, double minSpacingPx,
                                double minMinorSpacingPx = 4.0);

}  // namespace rocketplot::core
