#include "rocketplot/Range.h"

#include <limits>

#include <gtest/gtest.h>

namespace
{

using rocketplot::Range;

TEST(Range, EmptyIsTheIdentityForUnion)
{
    const Range r{.min = -2.0, .max = 5.0};
    EXPECT_EQ(Range::empty().united(r), r);
    EXPECT_EQ(r.united(Range::empty()), r);
    EXPECT_FALSE(Range::empty().isValid());
}

TEST(Range, IncludingGrowsAndIgnoresNonFinite)
{
    Range r = Range::empty().including(3.0);
    EXPECT_EQ(r, (Range{.min = 3.0, .max = 3.0}));
    r = r.including(-1.0).including(std::numeric_limits<double>::quiet_NaN());
    r = r.including(std::numeric_limits<double>::infinity());
    EXPECT_EQ(r, (Range{.min = -1.0, .max = 3.0}));
}

TEST(Range, Validity)
{
    EXPECT_TRUE((Range{.min = 1.0, .max = 1.0}).isValid());
    EXPECT_FALSE((Range{.min = 2.0, .max = 1.0}).isValid());
    EXPECT_FALSE((Range{.min = 0.0, .max = std::numeric_limits<double>::infinity()}).isValid());
    EXPECT_FALSE((Range{.min = std::numeric_limits<double>::quiet_NaN(), .max = 1.0}).isValid());
}

TEST(Range, ZoomKeepsTheAnchorFixed)
{
    const Range r{.min = 0.0, .max = 10.0};
    const Range zoomed = r.zoomedAbout(2.5, 0.5);
    EXPECT_DOUBLE_EQ(zoomed.min, 1.25);
    EXPECT_DOUBLE_EQ(zoomed.max, 6.25);
    // The anchor sits at the same fraction of the range before and after.
    EXPECT_DOUBLE_EQ((2.5 - zoomed.min) / zoomed.span(), (2.5 - r.min) / r.span());
}

TEST(Range, ShiftPadAndCenter)
{
    const Range r{.min = 1.0, .max = 3.0};
    EXPECT_EQ(r.shifted(2.0), (Range{.min = 3.0, .max = 5.0}));
    EXPECT_EQ(r.padded(0.5), (Range{.min = 0.0, .max = 4.0}));
    EXPECT_DOUBLE_EQ(r.center(), 2.0);
    const double big = std::numeric_limits<double>::max();
    EXPECT_DOUBLE_EQ((Range{.min = big / 2, .max = big}).center(), big * 0.75);  // no overflow
}

}  // namespace
