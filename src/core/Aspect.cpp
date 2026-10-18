#include "core/Aspect.h"

#include <cmath>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// @p range with @p span about its middle.
Range withSpan(Range range, double span)
{
    const double middle = range.center();
    return {.min = middle - (span / 2.0), .max = middle + (span / 2.0)};
}

}  // namespace

AspectRanges fitAspect(Range x, Range y, double width, double height, double ratio, AspectFit fit)
{
    const AspectRanges given{.x = x, .y = y};
    const auto         usable = [](double value) { return std::isfinite(value) && value > 0.0; };
    if (!usable(width) || !usable(height) || !usable(ratio) || !isUsableRange(x) ||
        !isUsableRange(y))
    {
        return given;
    }
    // Pixels per unit: width / x span along x, height / y span along y, and the second is ratio
    // times the first.
    const double ySpan = height * x.span() / (ratio * width);  // for x as it is
    const double xSpan = ratio * width * y.span() / height;    // for y as it is
    const bool   followX =
        fit == AspectFit::FOLLOW_X || (fit == AspectFit::WIDEN && ySpan > y.span());
    const AspectRanges fitted = followX ? AspectRanges{.x = x, .y = withSpan(y, ySpan)}
                                        : AspectRanges{.x = withSpan(x, xSpan), .y = y};
    return isUsableRange(fitted.x) && isUsableRange(fitted.y) ? fitted : given;
}

}  // namespace rocketplot::core
