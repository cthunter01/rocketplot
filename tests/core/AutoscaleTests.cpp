#include "core/Autoscale.h"

#include <limits>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::autoscaleRange;
using rocketplot::core::Scale;

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

TEST(Autoscale, DataCloseToZeroReachesIt)
{
    using rocketplot::core::ZeroRule;
    const ZeroRule zero{.includeWithin = 0.3};
    // 5 short of 0 is 5 / 95 of the span: within 0.3. 0 is exact, and the margin is all at the top.
    const Range near = autoscaleRange({.min = 5.0, .max = 100.0}, 0.03, Scale::LINEAR, zero);
    EXPECT_EQ(near.min, 0.0);
    EXPECT_DOUBLE_EQ(near.max, 103.0);
    // Below 0, the mirror image.
    const Range below = autoscaleRange({.min = -100.0, .max = -5.0}, 0.03, Scale::LINEAR, zero);
    EXPECT_DOUBLE_EQ(below.min, -103.0);
    EXPECT_EQ(below.max, 0.0);
    // 50 short is a whole span: too far.
    EXPECT_EQ(autoscaleRange({.min = 50.0, .max = 100.0}, 0.03, Scale::LINEAR, zero),
              autoscaleRange({.min = 50.0, .max = 100.0}, 0.03));
    // Data on both sides holds 0 already, and is padded as ever.
    EXPECT_EQ(autoscaleRange({.min = -10.0, .max = 100.0}, 0.03, Scale::LINEAR, zero),
              autoscaleRange({.min = -10.0, .max = 100.0}, 0.03));
    // Infinity always reaches it; 0 (the default), never.
    const ZeroRule always{.includeWithin = std::numeric_limits<double>::infinity()};
    EXPECT_EQ(autoscaleRange({.min = 50.0, .max = 50.0}, 0.1, Scale::LINEAR, always).min, 0.0);
    EXPECT_EQ(autoscaleRange({.min = 5.0, .max = 100.0}, 0.03, Scale::LINEAR, {}),
              autoscaleRange({.min = 5.0, .max = 100.0}, 0.03));
}

TEST(Autoscale, ABaselineStaysAtZero)
{
    using rocketplot::core::ZeroRule;
    const ZeroRule baseline{.baseline = true};
    const Range    up = autoscaleRange({.min = 0.0, .max = 100.0}, 0.05, Scale::LINEAR, baseline);
    EXPECT_EQ(up.min, 0.0);
    EXPECT_DOUBLE_EQ(up.max, 105.0);
    EXPECT_EQ(autoscaleRange({.min = -40.0, .max = 0.0}, 0.05, Scale::LINEAR, baseline).max, 0.0);
    // Only an end that is 0: one short of it is padded as ever.
    EXPECT_EQ(autoscaleRange({.min = 1.0, .max = 100.0}, 0.05, Scale::LINEAR, baseline),
              autoscaleRange({.min = 1.0, .max = 100.0}, 0.05));
    // All zeros: up from 0.
    EXPECT_EQ(autoscaleRange({.min = 0.0, .max = 0.0}, 0.05, Scale::LINEAR, baseline),
              (Range{.min = 0.0, .max = 1.0}));
}

TEST(Autoscale, ALogScaleHasNoZero)
{
    using rocketplot::core::ZeroRule;
    const ZeroRule zero{.includeWithin = 10.0, .baseline = true};
    EXPECT_EQ(autoscaleRange({.min = 5.0, .max = 100.0}, 0.03, Scale::LOG, zero),
              autoscaleRange({.min = 5.0, .max = 100.0}, 0.03, Scale::LOG));
}

// How far up @p range 0 is.
double zeroAt(Range range)
{
    return -range.min / range.span();
}

TEST(Autoscale, AlignedZerosWidenTheRangeWithZeroLower)
{
    using rocketplot::core::alignZeros;
    // 0 at the bottom of one and a little way up the other: the first grows down.
    const auto [left, right] = alignZeros({.min = -10.0, .max = 100.0}, {.min = 0.0, .max = 2.0});
    EXPECT_EQ(left, (Range{.min = -10.0, .max = 100.0}));
    EXPECT_DOUBLE_EQ(right.max, 2.0);
    EXPECT_DOUBLE_EQ(zeroAt(right), zeroAt(left));
    // In the other order, the same.
    const auto [first, second] = alignZeros({.min = 0.0, .max = 2.0}, {.min = -10.0, .max = 100.0});
    EXPECT_EQ(first, right);
    EXPECT_EQ(second, left);
}

TEST(Autoscale, AlignedZerosAtOppositeEndsGoToTheMiddle)
{
    using rocketplot::core::alignZeros;
    const auto [up, down] = alignZeros({.min = 0.0, .max = 4.0}, {.min = -3.0, .max = 0.0});
    EXPECT_EQ(up, (Range{.min = -4.0, .max = 4.0}));
    EXPECT_EQ(down, (Range{.min = -3.0, .max = 3.0}));
}

TEST(Autoscale, AlignedZerosFarApartBothGiveWay)
{
    using rocketplot::core::alignZeros;
    const auto [low, high] = alignZeros({.min = -1.0, .max = 9.0}, {.min = -9.0, .max = 1.0});
    EXPECT_DOUBLE_EQ(zeroAt(low), 0.5);
    EXPECT_DOUBLE_EQ(zeroAt(high), 0.5);
    EXPECT_EQ(low.max, 9.0);
    EXPECT_EQ(high.min, -9.0);
}

TEST(Autoscale, ZerosAreOnlyAlignedWhereBothRangesHoldOne)
{
    using rocketplot::core::alignZeros;
    const Range positive{.min = 5.0, .max = 10.0};
    const Range holding{.min = -1.0, .max = 1.0};
    const auto [a, b] = alignZeros(positive, holding);
    EXPECT_EQ(a, positive);
    EXPECT_EQ(b, holding);
    const auto [c, d] = alignZeros(holding, Range{.min = -3.0, .max = 3.0});  // aligned already
    EXPECT_EQ(c, holding);
    EXPECT_EQ(d, (Range{.min = -3.0, .max = 3.0}));
    const auto [e, f] = alignZeros(Range::empty(), holding);
    EXPECT_FALSE(e.isValid());
    EXPECT_EQ(f, holding);
}

}  // namespace
