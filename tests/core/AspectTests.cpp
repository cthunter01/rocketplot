#include "core/Aspect.h"

#include <array>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::AspectFit;
using rocketplot::core::AspectRanges;
using rocketplot::core::fitAspect;

// How far @p ranges are from @p ratio in a @p width by @p height plot area: 0 when to scale.
double aspectError(AspectRanges ranges, double width, double height, double ratio)
{
    const double xPerPixel = ranges.x.span() / width;
    const double yPerPixel = ranges.y.span() / height;
    return std::abs(xPerPixel - (ratio * yPerPixel)) / xPerPixel;
}

TEST(Aspect, WidenWidensTheRangeThatShowsLess)
{
    // A square of data in a wide plot area: x widens, about its middle.
    const AspectRanges wide = fitAspect({.min = 0.0, .max = 10.0}, {.min = 0.0, .max = 10.0}, 400.0,
                                        200.0, 1.0, AspectFit::WIDEN);
    EXPECT_EQ(wide.y, (Range{.min = 0.0, .max = 10.0}));
    EXPECT_DOUBLE_EQ(wide.x.min, -5.0);
    EXPECT_DOUBLE_EQ(wide.x.max, 15.0);
    // In a tall one, y.
    const AspectRanges tall = fitAspect({.min = 0.0, .max = 10.0}, {.min = 0.0, .max = 10.0}, 200.0,
                                        400.0, 1.0, AspectFit::WIDEN);
    EXPECT_EQ(tall.x, (Range{.min = 0.0, .max = 10.0}));
    EXPECT_DOUBLE_EQ(tall.y.min, -5.0);
    EXPECT_DOUBLE_EQ(tall.y.max, 15.0);
    EXPECT_LT(aspectError(tall, 200.0, 400.0, 1.0), 1e-12);
}

TEST(Aspect, TheRatioIsOfPixelsPerUnit)
{
    // A unit of y twice as tall as a unit of x is wide: a y span of 10 takes 200 pixels, so 100
    // pixels of width hold 10 units of x.
    const AspectRanges fitted = fitAspect({.min = 0.0, .max = 1.0}, {.min = 0.0, .max = 10.0},
                                          100.0, 200.0, 2.0, AspectFit::WIDEN);
    EXPECT_DOUBLE_EQ(fitted.x.span(), 10.0);
    EXPECT_LT(aspectError(fitted, 100.0, 200.0, 2.0), 1e-12);
}

TEST(Aspect, FollowingKeepsTheOtherRange)
{
    const Range x{.min = 0.0, .max = 100.0};
    const Range y{.min = 0.0, .max = 10.0};
    // y narrows to go with x's scale, about its middle.
    const AspectRanges followX = fitAspect(x, y, 300.0, 15.0, 1.0, AspectFit::FOLLOW_X);
    EXPECT_EQ(followX.x, x);
    EXPECT_DOUBLE_EQ(followX.y.min, 2.5);
    EXPECT_DOUBLE_EQ(followX.y.max, 7.5);
    const AspectRanges followY = fitAspect(x, y, 300.0, 15.0, 1.0, AspectFit::FOLLOW_Y);
    EXPECT_EQ(followY.y, y);
    EXPECT_DOUBLE_EQ(followY.x.span(), 200.0);
    EXPECT_DOUBLE_EQ(followY.x.center(), 50.0);
}

TEST(Aspect, WhatCannotBeFittedIsLeftAlone)
{
    const Range  x{.min = 0.0, .max = 1.0};
    const Range  y{.min = 0.0, .max = 2.0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto [width, height, ratio] :
         {std::array{0.0, 100.0, 1.0}, std::array{100.0, -1.0, 1.0}, std::array{100.0, 100.0, 0.0},
          std::array{100.0, 100.0, nan}})
    {
        const AspectRanges fitted = fitAspect(x, y, width, height, ratio, AspectFit::WIDEN);
        EXPECT_EQ(fitted.x, x);
        EXPECT_EQ(fitted.y, y);
    }
    const AspectRanges empty = fitAspect(Range::empty(), y, 100.0, 100.0, 1.0, AspectFit::WIDEN);
    EXPECT_EQ(empty.y, y);
}

}  // namespace
