#include "core/Autoscale.h"

#include <cmath>

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

}  // namespace

Range autoscaleRange(Range bounds, double margin, Scale scale)
{
    if (scale == Scale::LOG)
    {
        return logAutoscaleRange(bounds, margin);
    }
    if (!bounds.isValid())
    {
        return {.min = 0.0, .max = 1.0};
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

}  // namespace rocketplot::core
