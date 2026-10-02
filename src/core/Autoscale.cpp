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

}  // namespace

Range autoscaleRange(Range bounds, double margin)
{
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
    const Range padded = bounds.padded(std::isfinite(margin) && margin > 0.0 ? margin : 0.0);
    return isUsableRange(padded) ? padded : bounds;
}

}  // namespace rocketplot::core
