#include "core/Autoscale.h"

#include <limits>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::autoscaleRange;

TEST(Autoscale, PadsBySpanFraction)
{
    const Range r = autoscaleRange(Range{.min = 0.0, .max = 100.0}, 0.05);
    EXPECT_DOUBLE_EQ(r.min, -5.0);
    EXPECT_DOUBLE_EQ(r.max, 105.0);
}

TEST(Autoscale, NoDataGivesUnitRange)
{
    EXPECT_EQ(autoscaleRange(Range::empty(), 0.05), (Range{.min = 0.0, .max = 1.0}));
}

TEST(Autoscale, SingleValueGetsARangeAroundIt)
{
    const Range r = autoscaleRange(Range{.min = 50.0, .max = 50.0}, 0.05);
    EXPECT_DOUBLE_EQ(r.min, 45.0);
    EXPECT_DOUBLE_EQ(r.max, 55.0);
    EXPECT_EQ(autoscaleRange(Range{.min = 0.0, .max = 0.0}, 0.05),
              (Range{.min = -1.0, .max = 1.0}));
}

TEST(Autoscale, NegativeOrInvalidMarginMeansNone)
{
    EXPECT_EQ(autoscaleRange(Range{.min = 1.0, .max = 2.0}, -1.0), (Range{.min = 1.0, .max = 2.0}));
}

TEST(Autoscale, LogScalePadsMultiplicatively)
{
    using rocketplot::core::Scale;
    const Range r = autoscaleRange(Range{.min = 1.0, .max = 1e4}, 0.25, Scale::LOG);
    EXPECT_NEAR(r.min, 0.1, 1e-12);
    EXPECT_NEAR(r.max, 1e5, 1e-6);
    EXPECT_EQ(autoscaleRange(Range::empty(), 0.05, Scale::LOG), (Range{.min = 1.0, .max = 10.0}));
    EXPECT_EQ(autoscaleRange(Range{.min = 8.0, .max = 8.0}, 0.05, Scale::LOG),
              (Range{.min = 4.0, .max = 16.0}));
}

TEST(Autoscale, FollowShowsTheLatestWindow)
{
    using rocketplot::core::followRange;
    // Data shorter than the window: the window starts at the oldest point.
    EXPECT_EQ(followRange(Range{.min = 5.0, .max = 8.0}, 10.0, 0.0),
              (Range{.min = 5.0, .max = 15.0}));
    // Longer: it ends at the newest point, plus the margin.
    const Range r = followRange(Range{.min = 0.0, .max = 100.0}, 10.0, 0.1);
    EXPECT_DOUBLE_EQ(r.max, 101.0);
    EXPECT_DOUBLE_EQ(r.min, 91.0);
    EXPECT_EQ(followRange(Range::empty(), 10.0, 0.0), (Range{.min = 0.0, .max = 10.0}));
}

TEST(Autoscale, HeadroomLeavesTheTopShareFree)
{
    using rocketplot::core::withHeadroom;
    const Range grown = withHeadroom({.min = 0.0, .max = 75.0}, 0.25);
    EXPECT_DOUBLE_EQ(grown.min, 0.0);
    EXPECT_DOUBLE_EQ(grown.max, 100.0);
    // On a log scale, in decades: 1 to 100 is two of three.
    const Range log =
        withHeadroom({.min = 1.0, .max = 100.0}, 1.0 / 3.0, rocketplot::core::Scale::LOG);
    EXPECT_DOUBLE_EQ(log.min, 1.0);
    EXPECT_NEAR(log.max, 1000.0, 1e-9);
    // No share, or none that can be left free, changes nothing.
    for (const double share : {0.0, -0.1, 1.0, std::numeric_limits<double>::quiet_NaN()})
    {
        EXPECT_EQ(withHeadroom({.min = 2.0, .max = 3.0}, share), (Range{.min = 2.0, .max = 3.0}));
    }
}

}  // namespace
