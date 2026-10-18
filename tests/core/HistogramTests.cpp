#include "core/Histogram.h"

#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::Histogram;
using rocketplot::core::histogram;
using rocketplot::core::HistogramScale;

std::vector<double> oneToHundred()
{
    std::vector<double> values;
    values.reserve(100);
    for (int value = 1; value <= 100; ++value)
    {
        values.push_back(value);
    }
    return values;
}

double sum(const std::vector<double>& values)
{
    return std::accumulate(values.begin(), values.end(), 0.0);
}

TEST(Histogram, CountsEveryValueOnce)
{
    const Histogram counted = histogram(oneToHundred(), 10, {.min = 1.0, .max = 100.0});
    ASSERT_EQ(counted.edges.size(), 11U);
    ASSERT_EQ(counted.heights.size(), 10U);
    EXPECT_DOUBLE_EQ(sum(counted.heights), 100.0);
    // The ends exactly, the edges evenly between.
    EXPECT_EQ(counted.edges.front(), 1.0);
    EXPECT_EQ(counted.edges.back(), 100.0);
    EXPECT_DOUBLE_EQ(counted.edges[1], 10.9);
}

TEST(Histogram, TheLargestValueIsInTheLastBin)
{
    const Histogram counted =
        histogram(std::vector<double>{0.0, 1.0, 2.0}, 2, {.min = 0.0, .max = 2.0});
    EXPECT_EQ(counted.heights, (std::vector<double>{1.0, 2.0}));  // 1 is at an edge: the bin above
}

TEST(Histogram, NaNAndValuesOutsideAreLeftOut)
{
    const double    nan = std::numeric_limits<double>::quiet_NaN();
    const Histogram counted =
        histogram(std::vector<double>{nan, -1.0, 0.5, 3.0}, 2, {.min = 0.0, .max = 2.0});
    EXPECT_DOUBLE_EQ(sum(counted.heights), 1.0);
}

TEST(Histogram, ProbabilitiesAddUpToOneAndDensitiesAreaToOne)
{
    const Range     range{.min = 0.0, .max = 50.0};
    const Histogram probability = histogram(oneToHundred(), 5, range, HistogramScale::PROBABILITY);
    EXPECT_DOUBLE_EQ(sum(probability.heights), 1.0);  // of the 50 in the range
    const Histogram density = histogram(oneToHundred(), 5, range, HistogramScale::DENSITY);
    double          area    = 0.0;
    for (std::size_t i = 0; i < density.heights.size(); ++i)
    {
        area += density.heights[i] * (density.edges[i + 1] - density.edges[i]);
    }
    EXPECT_DOUBLE_EQ(area, 1.0);
}

TEST(Histogram, AutomaticBinCounts)
{
    using rocketplot::core::automaticBinCount;
    EXPECT_EQ(automaticBinCount(0), 5U);
    EXPECT_EQ(automaticBinCount(10), 5U);
    EXPECT_EQ(automaticBinCount(100), 10U);
    EXPECT_EQ(automaticBinCount(101), 11U);
    EXPECT_EQ(automaticBinCount(1'000'000), 50U);
}

TEST(Histogram, TheRangeOfAlikeValuesHasRoomAroundThem)
{
    using rocketplot::core::histogramRange;
    EXPECT_EQ(histogramRange(std::vector<double>{3.0, -1.0, 2.0}),
              (Range{.min = -1.0, .max = 3.0}));
    const Range alike = histogramRange(std::vector<double>{200.0, 200.0});
    EXPECT_DOUBLE_EQ(alike.min, 198.0);
    EXPECT_DOUBLE_EQ(alike.max, 202.0);
    const Range zeros = histogramRange(std::vector<double>{0.0});
    EXPECT_DOUBLE_EQ(zeros.min, -1e-9);
    EXPECT_DOUBLE_EQ(zeros.max, 1e-9);
    EXPECT_FALSE(
        histogramRange(std::vector<double>{std::numeric_limits<double>::quiet_NaN()}).isValid());
}

TEST(Histogram, NoBinsNoHistogram)
{
    EXPECT_TRUE(histogram(oneToHundred(), 0, {.min = 0.0, .max = 1.0}).edges.empty());
    EXPECT_TRUE(histogram(oneToHundred(), 5, Range::empty()).edges.empty());
}

}  // namespace
