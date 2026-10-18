#include "core/Autoscale.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// Half-width of the range around a single value v: 10% of |v|, or 1 around zero.
constexpr double kSingleValueFraction = 0.1;
constexpr double kSingleValueAtZero   = 1.0;
// On a log scale: a factor of 2 either side.
constexpr double kSingleValueFactor = 2.0;

double usableMargin(double margin)
{
    return std::isfinite(margin) && margin > 0.0 ? margin : 0.0;
}

Range logAutoscaleRange(Range bounds, double margin)
{
    if (!bounds.isValid() || !(bounds.min > 0.0))
    {
        return {.min = 1.0, .max = 10.0};
    }
    if (!isUsableRange(bounds, Scale::LOG))
    {
        return {.min = bounds.min / kSingleValueFactor, .max = bounds.max * kSingleValueFactor};
    }
    const double low  = std::log10(bounds.min);
    const double high = std::log10(bounds.max);
    const double pad  = (high - low) * usableMargin(margin);
    const Range  padded{.min = std::pow(10.0, low - pad), .max = std::pow(10.0, high + pad)};
    return isUsableRange(padded, Scale::LOG) ? padded : bounds;
}

// Which end of the data the zero rule takes to 0.
enum class ZeroEnd : std::uint8_t
{
    NONE,
    MIN,
    MAX,
};

ZeroEnd zeroEnd(Range bounds, ZeroRule zero)
{
    double  gap = 0.0;  // from the data to 0
    ZeroEnd end = ZeroEnd::NONE;
    if (bounds.min >= 0.0)
    {
        gap = bounds.min;
        end = ZeroEnd::MIN;
    }
    else if (bounds.max <= 0.0)
    {
        gap = -bounds.max;
        end = ZeroEnd::MAX;
    }
    else
    {
        return ZeroEnd::NONE;  // it holds 0 already
    }
    const double within  = zero.includeWithin;
    const bool   reaches = within > 0.0 && (std::isinf(within) || gap <= within * bounds.span());
    return reaches || (zero.baseline && gap == 0.0) ? end : ZeroEnd::NONE;
}

}  // namespace

Range autoscaleRange(Range bounds, double margin, Scale scale, ZeroRule zero)
{
    if (scale == Scale::LOG)
    {
        return logAutoscaleRange(bounds, margin);
    }
    if (!bounds.isValid())
    {
        return {.min = 0.0, .max = 1.0};
    }
    const ZeroEnd end = zeroEnd(bounds, zero);
    if (end != ZeroEnd::NONE)
    {
        bounds = bounds.including(0.0);
        if (!isUsableRange(bounds))
        {
            return {.min = 0.0, .max = 1.0};  // all zeros, from a baseline
        }
        const double pad    = bounds.span() * usableMargin(margin);
        const Range  padded = end == ZeroEnd::MIN ? Range{.min = 0.0, .max = bounds.max + pad}
                                                  : Range{.min = bounds.min - pad, .max = 0.0};
        return isUsableRange(padded) ? padded : bounds;
    }
    if (!isUsableRange(bounds))
    {
        // One value (or values too close to tell apart): center a sensible range on it.
        const double center = bounds.center();
        const double halfWidth =
            center == 0.0 ? kSingleValueAtZero : std::abs(center) * kSingleValueFraction;
        return {.min = center - halfWidth, .max = center + halfWidth};
    }
    const Range padded = bounds.padded(usableMargin(margin));
    return isUsableRange(padded) ? padded : bounds;
}

Range categoryRange(Range bounds, std::size_t count)
{
    // With none, as if there were one.
    const Range categories{.min = -0.5,
                           .max = static_cast<double>(std::max<std::size_t>(count, 1)) - 0.5};
    return categories.united(bounds);
}

Range followRange(Range bounds, double window, double margin)
{
    if (!(window > 0.0) || !std::isfinite(window))
    {
        return autoscaleRange(bounds, margin);
    }
    if (!bounds.isValid())
    {
        return {.min = 0.0, .max = window};
    }
    if (bounds.span() <= window * (1.0 - usableMargin(margin)))
    {
        return {.min = bounds.min, .max = bounds.min + window};
    }
    const double end = bounds.max + (window * usableMargin(margin));
    return {.min = end - window, .max = end};
}

AlignedZeros alignZeros(Range first, Range second)
{
    const auto holdsZero = [](Range range) {
        return isUsableRange(range) && range.min <= 0.0 && range.max >= 0.0;
    };
    if (!holdsZero(first) || !holdsZero(second))
    {
        return {.first = first, .second = second};
    }
    // How far up each range 0 is.
    const auto   zeroAt  = [](Range range) { return -range.min / range.span(); };
    const bool   swapped = zeroAt(first) > zeroAt(second);
    Range&       low     = swapped ? second : first;  // 0 the lower of the two
    Range&       high    = swapped ? first : second;
    const double lowAt   = zeroAt(low);
    const double highAt  = zeroAt(high);
    if (!(lowAt < highAt))
    {
        return {.first = first, .second = second};
    }
    // 0 goes to the same height t in both: the low range grows down to put it higher, by
    // (1 - lowAt) / (1 - t) - 1 of its span, the high range up to put it lower, by highAt / t - 1.
    // Their sum is least at t / (1 - t) = sqrt(highAt / (1 - lowAt)), within [lowAt, highAt].
    const double ratio  = std::sqrt(highAt / (1.0 - lowAt));
    const double target = std::clamp(ratio / (1.0 + ratio), lowAt, highAt);
    if (target > lowAt)
    {
        low.min = -target / (1.0 - target) * low.max;
    }
    if (target < highAt)
    {
        high.max = -high.min * (1.0 - target) / target;
    }
    return {.first = first, .second = second};
}

Range withHeadroom(Range range, double share, Scale scale)
{
    if (!(share > 0.0) || !(share < 1.0) || !isUsableRange(range, scale))  // NaN too
    {
        return range;
    }
    if (scale == Scale::LOG)
    {
        const double low   = std::log10(range.min);
        const double high  = std::log10(range.max);
        const Range  grown = {.min = range.min,
                              .max = std::pow(10.0, low + ((high - low) / (1.0 - share)))};
        return isUsableRange(grown, scale) ? grown : range;
    }
    const Range grown{.min = range.min, .max = range.min + (range.span() / (1.0 - share))};
    return isUsableRange(grown) ? grown : range;
}

}  // namespace rocketplot::core
