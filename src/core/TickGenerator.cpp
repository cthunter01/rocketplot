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
// Rather than show a single tick, ticks may come this much closer than asked.
constexpr double kCrowdedSpacing = 0.6;

// The mantissa of a nice step (1, 2 or 5): decides how a step is subdivided by minor ticks.
double stepMantissa(double step)
{
    const double exponent = std::floor(std::log10(step));
    return std::round(step / std::pow(10.0, exponent));
}

}  // namespace

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
    // A short axis may get a single tick (a step of 2 over -1.5 .. 1.6): take the next finer nice
    // step (5 → 2 → 1 → 0.5) while two or more ticks are still reasonably far apart.
    while (ticks.major.size() < 2)
    {
        const double finer = stepMantissa(ticks.step) == 5.0 ? ticks.step * 0.4 : ticks.step * 0.5;
        if (finer * lengthPx / range.span() < minSpacingPx * kCrowdedSpacing)
        {
            break;
        }
        ticks.step  = finer;
        ticks.major = multiplesOf(ticks.step, range);
    }
    const int subdivisions = stepMantissa(ticks.step) == 2.0 ? 4 : 5;
    if (ticks.step / static_cast<double>(subdivisions) * lengthPx / range.span() >=
        minMinorSpacingPx)
    {
        ticks.minor = minorMultiples(ticks.step, subdivisions, range);
    }
    return ticks;
}

std::vector<double> minorMultiples(double step, int subdivisions, Range range)
{
    std::vector<double> minor;
    for (const double value : multiplesOf(step / static_cast<double>(subdivisions), range))
    {
        // Skip the positions that coincide with major ticks.
        const double inSteps = value / step;
        if (std::abs(inSteps - std::round(inSteps)) > 1.0 / (2.0 * subdivisions))
        {
            minor.push_back(value);
        }
    }
    return minor;
}

bool fitsDecadeTicks(Range range)
{
    if (!range.isValid() || !(range.min > 0.0))
    {
        return false;
    }
    const double first = std::ceil(std::log10(range.min) - kStepTolerance);
    const double last  = std::floor(std::log10(range.max) + kStepTolerance);
    return last - first >= 1.0;
}

Ticks logTicks(Range range, double lengthPx, double minSpacingPx, double minMinorSpacingPx)
{
    Ticks ticks;
    if (!fitsDecadeTicks(range) || !(lengthPx > 0.0) || !(minSpacingPx > 0.0))
    {
        return ticks;
    }
    const double start       = std::log10(range.min);
    const double end         = std::log10(range.max);
    const double pxPerDecade = lengthPx / (end - start);
    const auto   first       = static_cast<std::int64_t>(std::ceil(start - kStepTolerance));
    const auto   last        = static_cast<std::int64_t>(std::floor(end + kStepTolerance));
    if (last - first > kMaxTicks)
    {
        return ticks;
    }
    // Every n-th power of ten (n = 1, 2, 5, 10, ...) so labels stay apart.
    ticks.step        = pxPerDecade >= minSpacingPx ? 1.0 : niceStep(minSpacingPx / pxPerDecade);
    const auto every  = static_cast<std::int64_t>(ticks.step);
    const auto decade = [](std::int64_t k) { return std::pow(10.0, static_cast<double>(k)); };
    // Within float noise of the range's ends still counts as inside.
    const auto inside = [&](double value) {
        return value >= range.min * (1.0 - kStepTolerance) &&
               value <= range.max * (1.0 + kStepTolerance);
    };
    for (std::int64_t k = first; k <= last; ++k)
    {
        if (((k % every) + every) % every == 0)
        {
            ticks.major.push_back(decade(k));
        }
        else if (pxPerDecade >= minMinorSpacingPx)
        {
            ticks.minor.push_back(decade(k));
        }
    }
    if (every > 1)
    {
        return ticks;
    }
    // Wide decades: 2x and 5x become major ticks (the narrowest gap of 1, 2, 5, 10 is 1 to 2, log 2
    // of a decade); all of 2x to 9x are minor ticks if 9x to 10x (log 10/9 of a decade) leaves
    // room.
    const bool twoAndFive = pxPerDecade * std::log10(2.0) >= minSpacingPx;
    const bool minor      = pxPerDecade * std::log10(10.0 / 9.0) >= minMinorSpacingPx;
    for (std::int64_t k = first - 1; k <= last; ++k)
    {
        for (int multiple = 2; multiple <= 9; ++multiple)
        {
            const double value = multiple * decade(k);
            if (!inside(value))
            {
                continue;
            }
            if (twoAndFive && (multiple == 2 || multiple == 5))
            {
                ticks.major.push_back(value);
            }
            else if (minor)
            {
                ticks.minor.push_back(value);
            }
        }
    }
    std::ranges::sort(ticks.major);
    std::ranges::sort(ticks.minor);
    return ticks;
}

}  // namespace rocketplot::core
