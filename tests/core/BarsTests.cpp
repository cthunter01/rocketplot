#include "core/Bars.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/NearestPoint.h"

namespace
{

using rocketplot::core::AxisMapping;
using rocketplot::core::BarRect;
using rocketplot::core::barRects;
using rocketplot::core::PixelBox;
using rocketplot::core::Scale;

// A plot area 300 by 200 pixels at (10, 10), x from 0 to 3 and y from -1 to 9.
const PixelBox    kClip{.left = 10.0, .top = 10.0, .right = 310.0, .bottom = 210.0};
const AxisMapping kX({.min = 0.0, .max = 3.0}, 10.0, 310.0);
const AxisMapping kY({.min = -1.0, .max = 9.0}, 210.0, 10.0);

bool isWhole(double pixel, double devicePixelRatio)
{
    const double device = pixel * devicePixelRatio;
    return std::abs(device - std::round(device)) < 1e-9;
}

// Expects @p rects to be on whole device pixels at @p devicePixelRatio, each touching the one
// before it.
void expectSnappedAndTouching(const std::vector<BarRect>& rects, double devicePixelRatio)
{
    for (std::size_t i = 0; i < rects.size(); ++i)
    {
        const PixelBox& box = rects[i].box;
        EXPECT_EQ(rects[i].index, i);
        EXPECT_TRUE(isWhole(box.left, devicePixelRatio) && isWhole(box.right, devicePixelRatio) &&
                    isWhole(box.top, devicePixelRatio) && isWhole(box.bottom, devicePixelRatio))
            << i << " at " << devicePixelRatio;
        if (i > 0)
        {
            EXPECT_EQ(box.left, rects[i - 1].box.right) << devicePixelRatio;
        }
    }
}

TEST(Bars, NeighboursShareTheirSnappedEdges)
{
    const std::vector<double> edges{0.0, 0.3337, 1.1, 2.05, 3.0};
    const std::vector<double> heights{1.0, 4.0, 2.5, 8.0};
    for (const double dpr : {1.0, 1.25})
    {
        std::vector<BarRect> rects;
        barRects(edges, heights, kX, kY, kClip, {.devicePixelRatio = dpr}, rects);
        ASSERT_EQ(rects.size(), 4U) << dpr;
        expectSnappedAndTouching(rects, dpr);
    }
    // Each stands on 0: y = 0 is 20 pixels above the bottom.
    std::vector<BarRect> rects;
    barRects(edges, heights, kX, kY, kClip, {}, rects);
    ASSERT_EQ(rects.size(), 4U);
    EXPECT_EQ(rects[0].box.bottom, 190.0);
    EXPECT_EQ(rects[3].box.top, 30.0);  // 8
}

TEST(Bars, AGapSeparatesThem)
{
    std::vector<BarRect> rects;
    barRects(std::vector<double>{0.0, 1.0, 2.0}, std::vector<double>{1.0, 1.0}, kX, kY, kClip,
             {.gap = 0.2}, rects);
    ASSERT_EQ(rects.size(), 2U);
    EXPECT_EQ(rects[0].box.left, 20.0);  // a tenth of 100 pixels on each side
    EXPECT_EQ(rects[0].box.right, 100.0);
    EXPECT_EQ(rects[1].box.left, 120.0);
}

TEST(Bars, BelowZeroTheyHangDown)
{
    std::vector<BarRect> rects;
    barRects(std::vector<double>{0.0, 1.0}, std::vector<double>{-1.0}, kX, kY, kClip, {}, rects);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0].box.top, 190.0);
    EXPECT_EQ(rects[0].box.bottom, 210.0);
}

TEST(Bars, NarrowBarsMergeAsTallAsTheTallest)
{
    // A thousand bins across 300 pixels.
    std::vector<double> edges;
    std::vector<double> heights;
    edges.reserve(1001);
    heights.reserve(1000);
    for (int i = 0; i <= 1000; ++i)
    {
        edges.push_back(3.0 * i / 1000.0);
    }
    for (int i = 0; i < 1000; ++i)
    {
        heights.push_back(i == 500 ? 8.0 : 1.0);
    }
    std::vector<BarRect> rects;
    barRects(edges, heights, kX, kY, kClip, {}, rects);
    EXPECT_LE(rects.size(), 300U);
    EXPECT_GE(rects.size(), 290U);
    double tallest = kClip.bottom;
    for (const BarRect& rect : rects)
    {
        EXPECT_GE(rect.box.right - rect.box.left, 1.0);
        tallest = std::min(tallest, rect.box.top);
    }
    EXPECT_EQ(tallest, 30.0);  // the one bar of 8 still shows
}

TEST(Bars, OnALogAxisTheyStandOnTheBottom)
{
    const AxisMapping    log({.min = 1.0, .max = 100.0}, 210.0, 10.0, Scale::LOG);
    std::vector<BarRect> rects;
    barRects(std::vector<double>{0.0, 1.0, 2.0, 3.0}, std::vector<double>{10.0, 0.0, -5.0}, kX, log,
             kClip, {}, rects);
    ASSERT_EQ(rects.size(), 1U);  // 0 and below can't be shown
    EXPECT_EQ(rects[0].box.bottom, 210.0);
    EXPECT_EQ(rects[0].box.top, 110.0);
}

TEST(Bars, NaNHeightsAreGaps)
{
    std::vector<BarRect> rects;
    barRects(std::vector<double>{0.0, 1.0, 2.0, 3.0},
             std::vector<double>{1.0, std::numeric_limits<double>::quiet_NaN(), 2.0}, kX, kY, kClip,
             {}, rects);
    ASSERT_EQ(rects.size(), 2U);
    EXPECT_EQ(rects[1].index, 2U);
}

TEST(Bars, BinAt)
{
    using rocketplot::core::binAt;
    const std::vector<double> edges{0.0, 1.0, 2.0};
    EXPECT_EQ(binAt(edges, 0.0), 0U);
    EXPECT_EQ(binAt(edges, 1.0), 1U);  // an edge belongs to the bin above it
    EXPECT_EQ(binAt(edges, 2.0), 1U);  // but the last one to the last bin
    EXPECT_FALSE(binAt(edges, 2.5));
    EXPECT_FALSE(binAt(edges, std::numeric_limits<double>::quiet_NaN()));
}

TEST(Bars, PickingSnapsToARectangleAndTracesABin)
{
    using rocketplot::core::pickBar;
    using rocketplot::core::PickQuery;
    const rocketplot::core::NearestPoint kNone{.index = 99, .pixel = {}, .distance = -1.0};
    const std::vector<double>            edges{0.0, 1.0, 2.0, 3.0};
    const std::vector<double>            heights{2.0, 6.0, 1.0};
    PickQuery query{.x = &kX, .y = &kY, .box = kClip, .position = {}, .reach = 10.0};
    // Inside the middle bar: on it, at the middle of its top.
    query.position                            = {.x = 150.0, .y = 150.0};
    const rocketplot::core::NearestPoint onIt = pickBar(edges, heights, query).value_or(kNone);
    EXPECT_EQ(onIt.index, 1U);
    EXPECT_EQ(onIt.distance, 0.0);
    EXPECT_DOUBLE_EQ(onIt.pixel.x, 160.0);
    EXPECT_DOUBLE_EQ(onIt.pixel.y, 70.0);
    // Above the low third bar, too far for a snap, but tracing goes to it.
    query.position = {.x = 250.0, .y = 40.0};
    EXPECT_FALSE(pickBar(edges, heights, query));
    query.tracing = true;
    EXPECT_EQ(pickBar(edges, heights, query).value_or(kNone).index, 2U);
}

}  // namespace
