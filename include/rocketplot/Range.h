#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace rocketplot
{

/// A closed interval [min, max] of data values, such as an axis's visible range or a series's
/// extent. Valid when both ends are finite and min <= max. Range::empty() is the identity for
/// united() and including().
struct Range
{
    double min = 0.0;
    double max = 0.0;

    /// The range that contains nothing: uniting it with another range gives that range.
    [[nodiscard]] static constexpr Range empty() noexcept
    {
        return {
            .min = std::numeric_limits<double>::infinity(),
            .max = -std::numeric_limits<double>::infinity(),
        };
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite(min) && std::isfinite(max) && min <= max;
    }
    [[nodiscard]] constexpr double span() const noexcept { return max - min; }
    /// Halves first, so the center of a range near the limits of double does not overflow.
    [[nodiscard]] constexpr double center() const noexcept { return (min * 0.5) + (max * 0.5); }
    [[nodiscard]] constexpr bool   contains(double value) const noexcept
    {
        return value >= min && value <= max;
    }

    /// This range grown to include @p value. A value that is not finite is ignored.
    [[nodiscard]] Range including(double value) const noexcept
    {
        if (!std::isfinite(value))
        {
            return *this;
        }
        return {.min = std::min(min, value), .max = std::max(max, value)};
    }
    /// The smallest range containing both. Range::empty() leaves the other unchanged.
    [[nodiscard]] constexpr Range united(Range other) const noexcept
    {
        return {.min = std::min(min, other.min), .max = std::max(max, other.max)};
    }
    /// Moved by @p delta.
    [[nodiscard]] constexpr Range shifted(double delta) const noexcept
    {
        return {.min = min + delta, .max = max + delta};
    }
    /// Scaled by @p factor about @p anchor, which keeps its relative position: factor < 1 zooms in.
    [[nodiscard]] constexpr Range zoomedAbout(double anchor, double factor) const noexcept
    {
        return {
            .min = anchor - ((anchor - min) * factor),
            .max = anchor + ((max - anchor) * factor),
        };
    }
    /// Widened by @p fraction of the span on each side.
    [[nodiscard]] constexpr Range padded(double fraction) const noexcept
    {
        const double pad = span() * fraction;
        return {.min = min - pad, .max = max + pad};
    }

    friend constexpr bool operator==(const Range&, const Range&) = default;
};

}  // namespace rocketplot
