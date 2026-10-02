#pragma once

#include <string>

namespace rocketplot::core
{

/// How the labels of one axis are written, chosen once for all its ticks so they look alike.
struct TickFormat
{
    bool scientific = false;  ///< 1.5e6 instead of 1500000
    int  decimals   = 0;      ///< Digits after the decimal point (of the mantissa when scientific)

    friend constexpr bool operator==(const TickFormat&, const TickFormat&) = default;
};

/// Decimal places needed to write every multiple of @p step exactly (0.25 → 2, 5 → 0, 0.1 → 1), at
/// most 15.
[[nodiscard]] int decimalsForStep(double step);

/// The format for ticks @p step apart on an axis whose largest absolute value is @p magnitude.
/// Scientific notation is used for steps of a million or more and for magnitudes below 1e-4.
[[nodiscard]] TickFormat chooseTickFormat(double step, double magnitude);

/// A tick label in UTF-8, e.g. "2.5", "−40" (with a true minus sign) or "3e6". Zero is always "0",
/// never "−0".
[[nodiscard]] std::string formatTick(double value, const TickFormat& format);

}  // namespace rocketplot::core
