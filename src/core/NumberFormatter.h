#pragma once

#include <cstdint>
#include <string>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// How an axis writes numbers.
enum class NumberStyle : std::uint8_t
{
    AUTO,   ///< Plain numbers, with a common offset and a ×10ⁿ multiplier when they'd be long
    SI,     ///< A common SI prefix on every label (250m, 1.5k), with an offset when needed
    PLAIN,  ///< Always the full number
};

/// How the labels of one linear axis are written: label = (value - offset) / 10^exponent with a
/// fixed number of decimals; or, when si is set, value - offset with each label's own SI prefix.
/// Chosen once per axis so all labels look alike.
struct Labeling
{
    double offset   = 0.0;
    int    exponent = 0;
    int    decimals = 0;
    bool   si       = false;
    double step     = 0.0;  ///< The tick step it was chosen for

    friend bool operator==(const Labeling&, const Labeling&) = default;
};

/// Decimal places needed to write every multiple of @p step exactly (0.25 → 2, 5 → 0, 0.1 → 1),
/// at most 15.
[[nodiscard]] int decimalsForStep(double step);

/// The labeling for ticks @p step apart on an axis showing @p range.
///
/// AUTO and SI subtract an offset when at least 4 leading digits are the same across the range
/// (epoch seconds, coordinates: 1700000000.1 to .9 becomes 0.1 to 0.9 "+1.7×10⁹"). AUTO then
/// factors out ×10ⁿ when the largest label would be 10⁶ or more, or below 10⁻⁴; SI uses the prefix
/// of the largest label's thousands.
[[nodiscard]] Labeling chooseLabeling(Range range, double step, NumberStyle style);

/// A tick label in UTF-8, e.g. "2.5", "−40" (with a true minus sign) or "250m". Zero is always "0".
[[nodiscard]] std::string formatLabel(double value, const Labeling& labeling);

/// What the labeling leaves out, to show once by the axis: "×10⁶", "+1.7×10⁹", "×10⁻³ +1.7×10⁹",
/// "+1.5k". Empty when labels are the full numbers.
[[nodiscard]] std::string labelingAnnotation(const Labeling& labeling);

/// A label for a tick on a log axis: plain for 0.001 to 10000 ("0.01", "100"), else "10⁻⁶" or
/// "2×10⁸"; with NumberStyle::SI, "100k", "2µ".
[[nodiscard]] std::string formatLogLabel(double value, NumberStyle style);

/// A value on its own, such as a crosshair's readout, written to @p resolution (the smallest
/// difference that matters, e.g. the value of one pixel): 3.14159 at 0.01 is "3.14". AUTO writes
/// numbers from 10⁻⁴ to 10⁶ plainly, and others as "1.235×10⁹" unless they need more than 6
/// digits (epoch seconds to the millisecond stay plain); SI uses a prefix ("1.235k"); PLAIN is
/// always plain.
[[nodiscard]] std::string formatValue(double value, double resolution, NumberStyle style);

/// @p exponent in Unicode superscript digits ("⁻¹²").
[[nodiscard]] std::string superscript(int exponent);

}  // namespace rocketplot::core
