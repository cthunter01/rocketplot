#include "core/TickGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// More ticks than this means the arguments were nonsense (a range far too large for its pixels).
constexpr std::int64_t kMaxTicks = 10'000;
// Tick positions are k * step; snap k to an integer when it is this close, so float noise in the
// range's ends doesn't drop the first or last tick.
constexpr double kStepTolerance = 1e-9;

// The mantissa of a nice step (1, 2 or 5): decides how a step is subdivided by minor ticks.
double stepMantissa(double step)
{
    const double exponent = std::floor(std::log10(step));
    return std::round(step / std::pow(10.0, exponent));
}

// k * step for k in [ceil(min/step), floor(max/step)], with values within float noise of zero made
// exactly 0. When the step's inverse is an integer (0.1, 0.05, ...), k / inverse is the double
// nearest the decimal value, where k * step can be off by an ulp (1.7e12 * 0.001 is just
// under 1.7e9).
std::vector<double> multiplesOf(double step, Range range)
{
    const double exactInverse = 1.0 / step;
    const double inverse      = step < 1.0 && std::abs(exactInverse - std::round(exactInverse)) <
                                                  kStepTolerance * exactInverse
                                    ? std::round(exactInverse)
                                    : 0.0;
    const auto   valueAt      = [&](double k) { return inverse > 0.0 ? k / inverse : k * step; };
    const double first        = std::ceil((range.min / step) - kStepTolerance);
    const double last         = std::floor((range.max / step) + kStepTolerance);
    if (!(last >= first) || last - first >= static_cast<double>(kMaxTicks))
    {
        return {};
    }
    const auto count = static_cast<std::int64_t>(last - first) + 1;

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (std::int64_t i = 0; i < count; ++i)
    {
        const double value = valueAt(first + static_cast<double>(i));
        values.push_back(std::abs(value) < step * kStepTolerance ? 0.0 : value);
    }
    return values;
}

}  // namespace

double niceStep(double rawStep)
{
    if (!(rawStep > 0.0) || !std::isfinite(rawStep))
    {
        return 0.0;
    }
    const double base     = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double mantissa = rawStep / base;
    double       nice     = 10.0;
    if (mantissa <= 1.0 + kStepTolerance)
    {
        nice = 1.0;
    }
    else if (mantissa <= 2.0 + kStepTolerance)
    {
        nice = 2.0;
    }
    else if (mantissa <= 5.0 + kStepTolerance)
    {
        nice = 5.0;
    }
    return nice * base;
}

Ticks linearTicks(Range range, double lengthPx, double minSpacingPx, double minMinorSpacingPx)
{
    Ticks ticks;
    if (!range.isValid() || !(range.span() > 0.0) || !(lengthPx > 0.0) || !(minSpacingPx > 0.0))
    {
        return ticks;
    }
    const double maxTickCount = std::max(1.0, std::floor(lengthPx / minSpacingPx));
    ticks.step                = niceStep(range.span() / maxTickCount);
    if (!(ticks.step > 0.0) || range.span() / ticks.step >
                                   static_cast<double>(kMaxTicks))  // the span is lost in precision
    {
        ticks.step = 0.0;
        return ticks;
    }
    ticks.major = multiplesOf(ticks.step, range);

    const auto   subdivisions = stepMantissa(ticks.step) == 2.0 ? 4 : 5;
    const double minorStep    = ticks.step / subdivisions;
    if (minorStep * lengthPx / range.span() >= minMinorSpacingPx)
    {
        for (const double value : multiplesOf(minorStep, range))
        {
            // Skip the minor positions that coincide with major ticks.
            const double inSteps = value / ticks.step;
            if (std::abs(inSteps - std::round(inSteps)) > 1.0 / (2.0 * subdivisions))
            {
                ticks.minor.push_back(value);
            }
        }
    }
    return ticks;
}

}  // namespace rocketplot::core
