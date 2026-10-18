#pragma once

#include <cstdint>
#include <span>
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
    LINEAR,    ///< Numbers on a linear scale
    LOG,       ///< Numbers on a log10 scale
    TIME,      ///< Seconds since the Unix epoch, UTC
    CATEGORY,  ///< Names: the one of index i at the value i
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
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::span<const std::string> categories{};  ///< For CATEGORY: the names
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
/// ticks. A category axis has a tick at each category in the range, labelled with its name; where
/// they would be closer than the spacing asked for, every n-th (from the first category).
[[nodiscard]] AxisTicks makeTicks(const TickRequest& request);

/// A single value of any kind of axis written out in full, to @p resolution (in data units, e.g.
/// what one pixel spans there): formatValue() for numbers, formatTime() for times, and the name of
/// the nearest category on a category axis (the number, where there is none).
[[nodiscard]] std::string formatReadout(double value, double resolution, TickKind kind,
                                        NumberStyle style, const UtcOffset& utcOffset = {},
                                        std::span<const std::string> categories = {});

}  // namespace rocketplot::core
