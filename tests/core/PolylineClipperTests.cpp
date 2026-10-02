#include "core/PolylineClipper.h"

#include <vector>

#include <gtest/gtest.h>

#include "core/Decimator.h"

namespace
{

using rocketplot::core::clipPolyline;
using rocketplot::core::clipSegment;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;

const PixelBox kBox{.left = 0.0, .top = 0.0, .right = 100.0, .bottom = 100.0};

Polyline makeRun(const std::vector<PixelPoint>& points)
{
    Polyline line;
    for (const PixelPoint& p : points)
    {
        line.add(p);
    }
    line.endRun();
    return line;
}

TEST(PolylineClipper, SegmentInsideIsUnchanged)
{
    PixelPoint a{.x = 10, .y = 10};
    PixelPoint b{.x = 90, .y = 50};
    EXPECT_TRUE(clipSegment(a, b, kBox));
    EXPECT_EQ(a, (PixelPoint{.x = 10, .y = 10}));
    EXPECT_EQ(b, (PixelPoint{.x = 90, .y = 50}));
}

TEST(PolylineClipper, SegmentCrossingIsCut)
{
    PixelPoint a{.x = -100, .y = 50};
    PixelPoint b{.x = 200, .y = 50};
    EXPECT_TRUE(clipSegment(a, b, kBox));
    EXPECT_EQ(a, (PixelPoint{.x = 0, .y = 50}));
    EXPECT_EQ(b, (PixelPoint{.x = 100, .y = 50}));
}

TEST(PolylineClipper, SegmentOutsideIsRejected)
{
    PixelPoint a{.x = -10, .y = -10};
    PixelPoint b{.x = -5, .y = 200};
    EXPECT_FALSE(clipSegment(a, b, kBox));
}

TEST(PolylineClipper, FarAwayPointsBecomeEdgePoints)
{
    // One point a million pixels away: the segment toward it ends on the box edge, with the same
    // direction.
    const Polyline in = makeRun({{.x = 50, .y = 50}, {.x = 1e6 + 50, .y = 50}});
    Polyline       out;
    clipPolyline(in, kBox, out);
    ASSERT_EQ(out.runCount(), 1U);
    const auto run = out.run(0);
    ASSERT_EQ(run.size(), 2U);
    EXPECT_EQ(run[1], (PixelPoint{.x = 100, .y = 50}));
}

TEST(PolylineClipper, LeavingAndReenteringSplitsTheRun)
{
    const Polyline in = makeRun({{.x = 10, .y = 50}, {.x = 50, .y = 200}, {.x = 90, .y = 50}});
    Polyline       out;
    clipPolyline(in, kBox, out);
    ASSERT_EQ(out.runCount(), 2U);
    EXPECT_EQ(out.run(0).front(), (PixelPoint{.x = 10, .y = 50}));
    EXPECT_EQ(out.run(1).back(), (PixelPoint{.x = 90, .y = 50}));
}

TEST(PolylineClipper, SinglePointsInsideAreKept)
{
    Polyline in;
    in.add({.x = 5, .y = 5});
    in.endRun();
    in.add({.x = 500, .y = 5});
    in.endRun();
    Polyline out;
    clipPolyline(in, kBox, out);
    ASSERT_EQ(out.runCount(), 1U);
    EXPECT_EQ(out.run(0).size(), 1U);
}

}  // namespace
