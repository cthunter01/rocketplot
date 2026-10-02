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
/// would be closer than @p minMinorSpacingPx. Major ticks that should be zero are exactly zero.
[[nodiscard]] Ticks linearTicks(Range range, double lengthPx, double minSpacingPx,
                                double minMinorSpacingPx = 4.0);

/// Whether a log axis showing @p range (all positive) can be labeled at powers of ten: it must
/// contain at least two of them. Narrower log ranges get linear ticks instead.
[[nodiscard]] bool fitsDecadeTicks(Range range);

/// Ticks for a logarithmic axis: at powers of ten (every n-th one if they would be closer than
/// @p minSpacingPx; Ticks::step is n), with 2x and 5x as major ticks too when decades are wide,
/// and minor ticks at the other multiples (or the skipped powers of ten). Empty unless
/// fitsDecadeTicks(range).
[[nodiscard]] Ticks logTicks(Range range, double lengthPx, double minSpacingPx,
                             double minMinorSpacingPx = 4.0);

/// k * step for every integer k with k * step in @p range (within float noise of its ends), each
/// the double nearest the exact decimal value. Values within float noise of zero are exactly 0.
[[nodiscard]] std::vector<double> multiplesOf(double step, Range range);

/// The multiples of step / subdivisions in @p range that aren't multiples of @p step.
[[nodiscard]] std::vector<double> minorMultiples(double step, int subdivisions, Range range);

}  // namespace rocketplot::core
