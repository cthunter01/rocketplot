#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// What the height of a histogram's bar is.
enum class HistogramScale : std::uint8_t
{
    COUNT,        ///< How many values fell in the bin
    PROBABILITY,  ///< The share of the values in the bins: the heights add up to 1
    DENSITY,      ///< The share per unit of x: the bars' areas add up to 1
};

/// A histogram: n bins between n + 1 edges, and their heights.
struct Histogram
{
    std::vector<double> edges;
    std::vector<double> heights;
};

/// The number of bins for @p count values when none is asked for: the square root, rounded up,
/// within 5 to 50.
[[nodiscard]] std::size_t automaticBinCount(std::size_t count);

/// The range of the finite @p values, for bins when none is asked for. All alike: a little either
/// side of them (1% of the value, at least 1e-9). No finite values: Range::empty().
[[nodiscard]] Range histogramRange(std::span<const double> values);

/// @p values counted into @p bins equal bins from @p range.min to @p range.max: a value at an edge
/// goes into the bin above it, except range.max itself, which goes into the last bin. Values
/// outside the range, and NaN, aren't counted. The first and last edges are the range's ends
/// exactly. With no bins or a range that isn't usable, the histogram is empty.
[[nodiscard]] Histogram histogram(std::span<const double> values, std::size_t bins, Range range,
                                  HistogramScale scale = HistogramScale::COUNT);

}  // namespace rocketplot::core
