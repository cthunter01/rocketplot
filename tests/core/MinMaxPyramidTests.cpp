#include "core/MinMaxPyramid.h"

#include <cstddef>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::kNoIndex;
using rocketplot::core::MinMax;
using rocketplot::core::MinMaxPyramid;

MinMax bruteForce(const std::vector<double>& y, std::size_t first, std::size_t last)
{
    MinMax result;
    for (std::size_t i = first; i < last; ++i)
    {
        result.add(i, y[i]);
    }
    return result;
}

void expectSameIndices(const MinMax& actual, const MinMax& expected)
{
    EXPECT_EQ(actual.hasNonFinite, expected.hasNonFinite);
    EXPECT_EQ(actual.firstFinite, expected.firstFinite);
    EXPECT_EQ(actual.lastFinite, expected.lastFinite);
    EXPECT_EQ(actual.argMin, expected.argMin);
    EXPECT_EQ(actual.argMax, expected.argMax);
}

void expectSame(const MinMax& actual, const MinMax& expected)
{
    expectSameIndices(actual, expected);
    if (expected.hasFinite())
    {
        EXPECT_EQ(actual.min, expected.min);
        EXPECT_EQ(actual.max, expected.max);
    }
}

std::vector<double> randomData(std::size_t count, unsigned seed, double nanFraction)
{
    std::mt19937 engine(seed);  // NOLINT(bugprone-random-generator-seed): reproducible test data
    std::normal_distribution<double> value(0.0, 1.0);
    std::bernoulli_distribution      nan(nanFraction);
    std::vector<double>              y(count);
    for (double& v : y)
    {
        v = nan(engine) ? std::numeric_limits<double>::quiet_NaN() : value(engine);
    }
    return y;
}

TEST(MinMaxPyramid, QueriesMatchBruteForce)
{
    const auto    y = randomData(300'000, 1, 0.001);
    MinMaxPyramid pyramid;
    pyramid.build(y);
    EXPECT_EQ(pyramid.levelCount(), 3U);  // 64, 4096, 262144

    std::mt19937 engine(2);  // NOLINT(bugprone-random-generator-seed): reproducible test data
    std::uniform_int_distribution<std::size_t> index(0, y.size());
    for (int trial = 0; trial < 500; ++trial)
    {
        std::size_t a = index(engine);
        std::size_t b = index(engine);
        if (a > b)
        {
            std::swap(a, b);
        }
        SCOPED_TRACE(testing::Message() << "[" << a << ", " << b << ")");
        expectSame(pyramid.query(y, a, b), bruteForce(y, a, b));
    }
    expectSame(pyramid.query(y, 0, y.size()), bruteForce(y, 0, y.size()));
}

TEST(MinMaxPyramid, ExtendEqualsRebuild)
{
    auto          y = randomData(1000, 3, 0.01);
    MinMaxPyramid grown;
    grown.build(y);
    for (int round = 0; round < 50; ++round)
    {
        const std::size_t oldSize = y.size();
        const auto        more    = randomData(997 + (static_cast<std::size_t>(round) * 61),
                                               10U + static_cast<unsigned>(round), 0.01);
        y.insert(y.end(), more.begin(), more.end());
        grown.extend(y, oldSize);
    }
    MinMaxPyramid rebuilt;
    rebuilt.build(y);
    ASSERT_EQ(grown.levelCount(), rebuilt.levelCount());
    for (std::size_t a = 0; a < y.size(); a += 4093)
    {
        expectSame(grown.query(y, a, y.size()), rebuilt.query(y, a, y.size()));
        expectSame(grown.query(y, a, y.size()), bruteForce(y, a, y.size()));
    }
}

TEST(MinMaxPyramid, AllNonFinite)
{
    const std::vector<double> y(200, std::numeric_limits<double>::infinity());
    MinMaxPyramid             pyramid;
    pyramid.build(y);
    const MinMax summary = pyramid.query(y, 0, y.size());
    EXPECT_FALSE(summary.hasFinite());
    EXPECT_TRUE(summary.hasNonFinite);
    EXPECT_EQ(summary.argMin, kNoIndex);
}

TEST(MinMaxPyramid, EmptyAndSmall)
{
    const std::vector<double> y{3.0, 1.0, 2.0};
    MinMaxPyramid             pyramid;
    pyramid.build(y);
    EXPECT_EQ(pyramid.levelCount(), 0U);
    const MinMax summary = pyramid.query(y, 0, 3);
    EXPECT_EQ(summary.argMin, 1U);
    EXPECT_EQ(summary.argMax, 0U);
    EXPECT_FALSE(pyramid.query(y, 1, 1).hasFinite());
}

}  // namespace
