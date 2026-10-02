#include "core/TickGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::linearTicks;
using rocketplot::core::niceStep;

TEST(TickGenerator, NiceStepsAreOneTwoFive)
{
    EXPECT_DOUBLE_EQ(niceStep(0.7), 1.0);
    EXPECT_DOUBLE_EQ(niceStep(1.0), 1.0);
    EXPECT_DOUBLE_EQ(niceStep(1.3), 2.0);
    EXPECT_DOUBLE_EQ(niceStep(2.1), 5.0);
    EXPECT_DOUBLE_EQ(niceStep(5.5), 10.0);
    EXPECT_DOUBLE_EQ(niceStep(0.03), 0.05);
    EXPECT_DOUBLE_EQ(niceStep(4200.0), 5000.0);
    EXPECT_DOUBLE_EQ(niceStep(0.0), 0.0);
    EXPECT_DOUBLE_EQ(niceStep(-1.0), 0.0);
}

TEST(TickGenerator, CoversTheRangeWithNiceValues)
{
    const auto ticks = linearTicks(Range{.min = -3.6, .max = 123.6}, 800.0, 100.0);
    EXPECT_DOUBLE_EQ(ticks.step, 20.0);
    const std::vector<double> expected{0.0, 20.0, 40.0, 60.0, 80.0, 100.0, 120.0};
    EXPECT_EQ(ticks.major, expected);
}

TEST(TickGenerator, RespectsTheMinimumSpacing)
{
    const Range range{.min = 0.0, .max = 1.0};
    for (const double length : {100.0, 200.0, 777.0, 3000.0})
    {
        const auto   ticks   = linearTicks(range, length, 60.0);
        const double spacing = ticks.step / range.span() * length;
        EXPECT_GE(spacing, 60.0) << "length " << length;
        EXPECT_GE(ticks.major.size(), 1U);
    }
}

TEST(TickGenerator, ZeroIsExactlyZero)
{
    const auto ticks   = linearTicks(Range{.min = -0.3, .max = 0.3}, 600.0, 50.0);
    bool       sawZero = false;
    for (const double value : ticks.major)
    {
        if (std::abs(value) < 1e-12)
        {
            EXPECT_EQ(value, 0.0);
            EXPECT_FALSE(std::signbit(value));
            sawZero = true;
        }
    }
    EXPECT_TRUE(sawZero);
}

TEST(TickGenerator, IncludesTicksOnTheRangeEnds)
{
    // 0.1 * 3 is 0.30000000000000004; the tick at the end must not be lost to that.
    const auto ticks = linearTicks(Range{.min = 0.0, .max = 0.3}, 300.0, 100.0);
    ASSERT_FALSE(ticks.major.empty());
    EXPECT_DOUBLE_EQ(ticks.major.front(), 0.0);
    EXPECT_NEAR(ticks.major.back(), 0.3, 1e-12);
}

TEST(TickGenerator, MinorTicksSubdivideAndSkipMajors)
{
    const auto ticks = linearTicks(Range{.min = 0.0, .max = 10.0}, 1000.0, 200.0);
    EXPECT_DOUBLE_EQ(ticks.step, 2.0);
    // A step of 2 is divided in 4: minor ticks every 0.5, except on the majors.
    EXPECT_EQ(ticks.minor.size(), 15U);
    for (const double value : ticks.minor)
    {
        EXPECT_NE(std::fmod(value, 2.0), 0.0) << value;
    }
}

TEST(TickGenerator, NoMinorTicksWhenTooDense)
{
    // Minor ticks would be 5 px apart.
    const auto ticks = linearTicks(Range{.min = 0.0, .max = 10.0}, 100.0, 20.0, 6.0);
    EXPECT_TRUE(ticks.minor.empty());
}

TEST(TickGenerator, LargeOffsetsAndTinySpans)
{
    const auto ticks = linearTicks(Range{.min = 1.7e9, .max = 1.7e9 + 0.01}, 1000.0, 100.0);
    ASSERT_GE(ticks.major.size(), 2U);
    EXPECT_DOUBLE_EQ(ticks.step, 0.001);
    for (const double value : ticks.major)
    {
        EXPECT_GE(value, 1.7e9);
        EXPECT_LE(value, 1.7e9 + 0.01);
    }
}

TEST(TickGenerator, DegenerateInputsGiveNoTicks)
{
    EXPECT_TRUE(linearTicks(Range{.min = 1.0, .max = 1.0}, 100.0, 10.0).major.empty());
    EXPECT_TRUE(linearTicks(Range::empty(), 100.0, 10.0).major.empty());
    EXPECT_TRUE(linearTicks(Range{.min = 0.0, .max = 1.0}, 0.0, 10.0).major.empty());
    EXPECT_TRUE(linearTicks(Range{.min = 0.0, .max = 1.0}, 100.0, 0.0).major.empty());
}

TEST(TickGenerator, NegativeRanges)
{
    const auto                ticks = linearTicks(Range{.min = -50.0, .max = -10.0}, 400.0, 100.0);
    const std::vector<double> expected{-50.0, -40.0, -30.0, -20.0, -10.0};
    EXPECT_EQ(ticks.major, expected);
}

TEST(TickGenerator, LogTicksAtPowersOfTen)
{
    using rocketplot::core::logTicks;
    // Six decades over 600 px, labels 50 px apart: every power of ten, plus 2x and 5x? A decade is
    // 100 px; 1 to 2 is 30 px, too close.
    const auto                ticks = logTicks(Range{.min = 1e-3, .max = 1e3}, 600.0, 50.0, 4.0);
    const std::vector<double> expected{1e-3, 1e-2, 1e-1, 1.0, 10.0, 100.0, 1000.0};
    ASSERT_EQ(ticks.major.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_DOUBLE_EQ(ticks.major[i], expected[i]);
    }
    // Minor ticks at 2..9 of each decade: 9 to 10 is 4.6 px apart.
    EXPECT_EQ(ticks.minor.size(), 6U * 8U);
}

TEST(TickGenerator, LogTicksSkipDecadesWhenCrowded)
{
    using rocketplot::core::logTicks;
    const auto ticks = logTicks(Range{.min = 1e-10, .max = 1e10}, 200.0, 40.0, 4.0);
    EXPECT_DOUBLE_EQ(ticks.step, 5.0);  // every 5th decade
    for (const double value : ticks.major)
    {
        const double exponent = std::log10(value);
        EXPECT_NEAR(std::fmod(std::abs(exponent), 5.0), 0.0, 1e-9) << value;
    }
}

TEST(TickGenerator, WideLogDecadesGetTwoAndFive)
{
    using rocketplot::core::logTicks;
    const auto ticks = logTicks(Range{.min = 1.0, .max = 100.0}, 800.0, 60.0, 4.0);
    EXPECT_NE(std::ranges::find(ticks.major, 2.0), ticks.major.end());
    EXPECT_NE(std::ranges::find(ticks.major, 50.0), ticks.major.end());
    EXPECT_TRUE(std::ranges::is_sorted(ticks.major));
}

TEST(TickGenerator, NarrowLogRangesDontFitDecades)
{
    EXPECT_FALSE(rocketplot::core::fitsDecadeTicks(Range{.min = 20.0, .max = 80.0}));
    EXPECT_FALSE(rocketplot::core::fitsDecadeTicks(Range{.min = 2.0, .max = 50.0}));
    EXPECT_TRUE(rocketplot::core::fitsDecadeTicks(Range{.min = 0.5, .max = 10.0}));
    EXPECT_FALSE(rocketplot::core::fitsDecadeTicks(Range{.min = -1.0, .max = 100.0}));
}

TEST(TickGenerator, AtLeastTwoTicksWhenTheyFitLoosely)
{
    // 130 px for -1.5 .. 1.6 with labels 33 px apart: a step of 2 would leave only 0.
    const auto ticks = linearTicks(Range{.min = -1.5, .max = 1.6}, 130.0, 33.0);
    EXPECT_GE(ticks.major.size(), 2U);
    EXPECT_DOUBLE_EQ(ticks.step, 1.0);
}

}  // namespace
