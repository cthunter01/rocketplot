#include "core/AxisMapping.h"

#include <limits>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::AxisMapping;

TEST(AxisMapping, MapsEndsAndRoundTrips)
{
    const AxisMapping x(Range{.min = 10.0, .max = 20.0}, 100.0, 300.0);
    EXPECT_DOUBLE_EQ(x.toPixel(10.0), 100.0);
    EXPECT_DOUBLE_EQ(x.toPixel(20.0), 300.0);
    EXPECT_DOUBLE_EQ(x.toPixel(15.0), 200.0);
    EXPECT_DOUBLE_EQ(x.toValue(x.toPixel(12.345)), 12.345);
    EXPECT_DOUBLE_EQ(x.pixelLength(), 200.0);
}

TEST(AxisMapping, YAxisGrowsUpward)
{
    const AxisMapping y(Range{.min = 0.0, .max = 1.0}, 400.0, 0.0);
    EXPECT_DOUBLE_EQ(y.toPixel(0.0), 400.0);
    EXPECT_DOUBLE_EQ(y.toPixel(1.0), 0.0);
    EXPECT_DOUBLE_EQ(y.toValue(100.0), 0.75);
}

TEST(AxisMapping, PanFollowsThePointer)
{
    const AxisMapping x(Range{.min = 0.0, .max = 100.0}, 0.0, 1000.0);
    // Dragging the content 100 px right shows values 10 lower.
    const Range panned = rocketplot::core::pannedRange(x, 100.0);
    EXPECT_DOUBLE_EQ(panned.min, -10.0);
    EXPECT_DOUBLE_EQ(panned.max, 90.0);

    const AxisMapping y(Range{.min = 0.0, .max = 100.0}, 1000.0, 0.0);
    // Dragging the content 100 px down shows values 10 higher.
    const Range pannedY = rocketplot::core::pannedRange(y, 100.0);
    EXPECT_DOUBLE_EQ(pannedY.min, 10.0);
    EXPECT_DOUBLE_EQ(pannedY.max, 110.0);
}

TEST(AxisMapping, ZoomKeepsTheValueUnderThePixel)
{
    const AxisMapping x(Range{.min = 0.0, .max = 100.0}, 0.0, 1000.0);
    const Range       zoomed = rocketplot::core::zoomedRange(x, 250.0, 0.5);
    const AxisMapping after(zoomed, 0.0, 1000.0);
    EXPECT_DOUBLE_EQ(after.toValue(250.0), 25.0);
    EXPECT_DOUBLE_EQ(zoomed.span(), 50.0);
}

TEST(AxisMapping, DegenerateRangeDoesNotDivideByZero)
{
    const AxisMapping x(Range{.min = 5.0, .max = 5.0}, 0.0, 100.0);
    EXPECT_DOUBLE_EQ(x.toPixel(5.0), 0.0);
}

TEST(AxisMapping, UsableRanges)
{
    using rocketplot::core::isUsableRange;
    EXPECT_TRUE(isUsableRange({.min = 0.0, .max = 1.0}));
    EXPECT_TRUE(
        isUsableRange({.min = 1.7e9, .max = 1.7e9 + 0.001}));  // epoch seconds, a millisecond wide
    EXPECT_FALSE(isUsableRange({.min = 1.0, .max = 1.0}));
    EXPECT_FALSE(isUsableRange({.min = 1e9, .max = 1e9 + 1e-9}));  // below double resolution
    EXPECT_FALSE(isUsableRange({.min = -1e301, .max = 1e301}));
    EXPECT_FALSE(isUsableRange({.min = 0.0, .max = std::numeric_limits<double>::infinity()}));
}

}  // namespace
