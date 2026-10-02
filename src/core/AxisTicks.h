#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/NumberFormatter.h"
#include "core/TimeTicks.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// What an axis's values are.
enum class TickKind : std::uint8_t
{
    LINEAR,  ///< Numbers on a linear scale
    LOG,     ///< Numbers on a log10 scale
    TIME,    ///< Seconds since the Unix epoch, UTC
};

struct TickRequest
{
    Range       range{};
    TickKind    kind              = TickKind::LINEAR;
    NumberStyle style             = NumberStyle::AUTO;  ///< For numbers
    double      lengthPx          = 0.0;
    double      minSpacingPx      = 0.0;  ///< Between major ticks
    double      minMinorSpacingPx = 4.0;
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    UtcOffset utcOffset{};  ///< For TIME: the time zone (empty: UTC)
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::string zoneName{};  ///< For TIME: appended to the context ("2026-03-28 UTC")
};

/// One axis's ticks with their labels, and the text shown once by the axis for what the labels
/// leave out: an offset or ×10ⁿ multiplier for numbers, the date for times of day.
struct AxisTicks
{
    std::vector<double>      major;
    std::vector<double>      minor;
    std::vector<std::string> labels;  ///< One per major tick
    std::string              annotation;
};

/// Ticks and labels for any kind of axis. Log axes narrower than two powers of ten get linear
/// ticks.
[[nodiscard]] AxisTicks makeTicks(const TickRequest& request);

}  // namespace rocketplot::core
