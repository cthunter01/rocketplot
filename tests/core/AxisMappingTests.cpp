#include "core/AxisMapping.h"

#include <cmath>
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

TEST(AxisMapping, LogScaleSpacesDecadesEvenly)
{
    using rocketplot::core::Scale;
    const AxisMapping x(Range{.min = 1.0, .max = 1000.0}, 0.0, 300.0, Scale::LOG);
    EXPECT_DOUBLE_EQ(x.toPixel(1.0), 0.0);
    EXPECT_NEAR(x.toPixel(10.0), 100.0, 1e-9);
    EXPECT_NEAR(x.toPixel(100.0), 200.0, 1e-9);
    EXPECT_NEAR(x.toValue(150.0), std::sqrt(1000.0), 1e-9);
    EXPECT_TRUE(std::isnan(x.toPixel(-1.0)));
    EXPECT_FALSE(std::isfinite(x.toPixel(0.0)));
}

TEST(AxisMapping, LogZoomAndPanAreMultiplicative)
{
    using rocketplot::core::Scale;
    const AxisMapping x(Range{.min = 1.0, .max = 10000.0}, 0.0, 400.0, Scale::LOG);
    const Range       zoomed = rocketplot::core::zoomedRange(x, 200.0, 0.5);  // about 100
    EXPECT_NEAR(zoomed.min, 10.0, 1e-9);
    EXPECT_NEAR(zoomed.max, 1000.0, 1e-9);
    const Range panned = rocketplot::core::pannedRange(x, -100.0);  // one decade
    EXPECT_NEAR(panned.min, 10.0, 1e-9);
    EXPECT_NEAR(panned.max, 100000.0, 1e-6);
}

TEST(AxisMapping, UsableLogRanges)
{
    using rocketplot::core::isUsableRange;
    using rocketplot::core::Scale;
    EXPECT_TRUE(isUsableRange({.min = 1e-300, .max = 1e300}, Scale::LOG));
    EXPECT_FALSE(isUsableRange({.min = 0.0, .max = 10.0}, Scale::LOG));
    EXPECT_FALSE(isUsableRange({.min = -1.0, .max = 10.0}, Scale::LOG));
    EXPECT_TRUE(isUsableRange({.min = -1.0, .max = 10.0}, Scale::LINEAR));
}

TEST(AxisMapping, TransformMovesAndZoomsAtOnce)
{
    using rocketplot::core::pannedRange;
    using rocketplot::core::transformedRange;
    using rocketplot::core::zoomedRange;
    const AxisMapping x(Range{.min = 0.0, .max = 100.0}, 0.0, 1000.0);
    // Scale 1 is a pan; equal pixels a zoom.
    EXPECT_EQ(transformedRange(x, 500.0, 600.0, 1.0), pannedRange(x, 100.0));
    const Range zoom = transformedRange(x, 250.0, 250.0, 2.0);
    EXPECT_DOUBLE_EQ(zoom.min, zoomedRange(x, 250.0, 0.5).min);
    EXPECT_DOUBLE_EQ(zoom.max, zoomedRange(x, 250.0, 0.5).max);
    // Both: the value at 200 px (20) lands at 600 px, magnified twice.
    const Range       both = transformedRange(x, 200.0, 600.0, 2.0);
    const AxisMapping after(both, 0.0, 1000.0);
    EXPECT_NEAR(after.toPixel(20.0), 600.0, 1e-9);
    EXPECT_NEAR(both.span(), 50.0, 1e-9);
    // Nothing to scale by, or nothing to do: unchanged.
    EXPECT_EQ(transformedRange(x, 0.0, 10.0, 0.0), x.range());
    const AxisMapping odd(Range{.min = -5.3, .max = 5.3}, 517.0, 13.0);
    EXPECT_EQ(transformedRange(odd, 211.0, 211.0, 1.0), odd.range());
}

TEST(AxisMapping, TransformOnAYAxisAndALogScale)
{
    using rocketplot::core::Scale;
    using rocketplot::core::transformedRange;
    const AxisMapping y(Range{.min = 0.0, .max = 1.0}, 400.0, 0.0);
    // Content dragged 100 px down: higher values come into view at the top.
    const Range down = transformedRange(y, 200.0, 300.0, 1.0);
    EXPECT_DOUBLE_EQ(down.min, 0.25);
    EXPECT_DOUBLE_EQ(down.max, 1.25);
    const AxisMapping log(Range{.min = 1.0, .max = 10000.0}, 0.0, 400.0, Scale::LOG);
    const Range       zoomed = transformedRange(log, 200.0, 200.0, 2.0);  // about 100
    EXPECT_NEAR(zoomed.min, 10.0, 1e-9);
    EXPECT_NEAR(zoomed.max, 1000.0, 1e-9);
}

TEST(AxisMapping, RangeBetweenPixels)
{
    const AxisMapping y(Range{.min = 0.0, .max = 1.0}, 400.0, 0.0);
    const Range       box = rocketplot::core::rangeBetween(y, 100.0, 300.0);
    EXPECT_DOUBLE_EQ(box.min, 0.25);
    EXPECT_DOUBLE_EQ(box.max, 0.75);
}

}  // namespace
