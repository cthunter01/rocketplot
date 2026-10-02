#include "core/Autoscale.h"

#include <gtest/gtest.h>

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

}  // namespace
