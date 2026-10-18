#include "core/Histogram.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

constexpr std::size_t kMinBins = 5;
constexpr std::size_t kMaxBins = 50;
// A range around values that are all alike: this share of them, or at least kMinHalfWidth.
constexpr double kAlikeShare   = 0.01;
constexpr double kMinHalfWidth = 1e-9;

}  // namespace

std::size_t automaticBinCount(std::size_t count)
{
    const auto root = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    return std::clamp(root, kMinBins, kMaxBins);
}

Range histogramRange(std::span<const double> values)
{
    Range range = Range::empty();
    for (const double value : values)
    {
        if (std::isfinite(value))
        {
            range = range.including(value);
        }
    }
    if (range.isValid() && !(range.max > range.min))
    {
        const double half = std::max(kMinHalfWidth, kAlikeShare * std::abs(range.min));
        return {.min = range.min - half, .max = range.max + half};
    }
    return range;
}

Histogram histogram(std::span<const double> values, std::size_t bins, Range range,
                    HistogramScale scale)
{
    Histogram result;
    if (bins == 0 || !isUsableRange(range))
    {
        return result;
    }
    const double width = range.span() / static_cast<double>(bins);
    result.edges.reserve(bins + 1);
    for (std::size_t i = 0; i < bins; ++i)
    {
        result.edges.push_back(range.min + (static_cast<double>(i) * width));
    }
    result.edges.push_back(range.max);
    result.heights.assign(bins, 0.0);
    double counted = 0.0;
    for (const double value : values)
    {
        if (!(value >= range.min) || !(value <= range.max))  // NaN too
        {
            continue;
        }
        // Rounding may put a value just past an edge: the edges decide.
        auto bin = std::min(static_cast<std::size_t>((value - range.min) / width), bins - 1);
        while (bin > 0 && value < result.edges[bin])
        {
            --bin;
        }
        while (bin + 1 < bins && value >= result.edges[bin + 1])
        {
            ++bin;
        }
        result.heights[bin] += 1.0;
        counted += 1.0;
    }
    if (scale == HistogramScale::COUNT || counted == 0.0)
    {
        return result;
    }
    for (std::size_t i = 0; i < bins; ++i)
    {
        result.heights[i] /= counted;
        if (scale == HistogramScale::DENSITY)
        {
            result.heights[i] /= result.edges[i + 1] - result.edges[i];
        }
    }
    return result;
}

}  // namespace rocketplot::core
