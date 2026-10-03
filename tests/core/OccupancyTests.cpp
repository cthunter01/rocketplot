#include "core/Occupancy.h"

#include <initializer_list>
#include <vector>

#include <gtest/gtest.h>

#include "core/Decimator.h"

namespace
{

using rocketplot::core::Occupancy;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;

constexpr PixelBox kArea{.left = 0.0, .top = 0.0, .right = 400.0, .bottom = 200.0};

Polyline line(std::initializer_list<PixelPoint> points)
{
    Polyline result;
    for (const PixelPoint point : points)
    {
        result.add(point);
    }
    result.endRun();
    return result;
}

TEST(Occupancy, EmptyUntilRecorded)
{
    Occupancy occupancy;
    EXPECT_FALSE(occupancy.isRecorded());
    EXPECT_DOUBLE_EQ(occupancy.coverage(kArea), 0.0);
    occupancy.reset(kArea);
    EXPECT_TRUE(occupancy.isRecorded());
    EXPECT_DOUBLE_EQ(occupancy.coverage(kArea), 0.0);
    occupancy.clear();
    EXPECT_FALSE(occupancy.isRecorded());
}

TEST(Occupancy, ADiagonalCoversTheBoxesItCrosses)
{
    Occupancy occupancy;
    occupancy.reset(kArea);
    // From the bottom left to the top right.
    occupancy.addPolyline(line({{.x = 0.0, .y = 200.0}, {.x = 400.0, .y = 0.0}}), 2.0);
    const PixelBox topLeft{.left = 10.0, .top = 10.0, .right = 110.0, .bottom = 60.0};
    const PixelBox topRight{.left = 290.0, .top = 10.0, .right = 390.0, .bottom = 60.0};
    EXPECT_DOUBLE_EQ(occupancy.coverage(topLeft), 0.0);
    EXPECT_GT(occupancy.coverage(topRight), 0.0);
    EXPECT_LT(occupancy.coverage(topRight), 0.5);  // a line, not the whole box
}

TEST(Occupancy, DenseDataFillsItsSpan)
{
    Occupancy occupancy;
    occupancy.reset(kArea);
    // Min/max per pixel column between y = 100 and 150, as decimated noise looks.
    Polyline noise;
    for (int x = 0; x < 400; ++x)
    {
        noise.add({.x = x + 0.25, .y = 100.0});
        noise.add({.x = x + 0.75, .y = 150.0});
    }
    noise.endRun();
    occupancy.addPolyline(noise, 1.0);
    EXPECT_GT(occupancy.coverage({.left = 100.0, .top = 104.0, .right = 300.0, .bottom = 148.0}),
              0.99);
    EXPECT_DOUBLE_EQ(occupancy.coverage({.left = 0.0, .top = 0.0, .right = 400.0, .bottom = 90.0}),
                     0.0);
}

TEST(Occupancy, PointsAndLoneSamples)
{
    Occupancy occupancy;
    occupancy.reset(kArea);
    occupancy.addPoints(std::vector<PixelPoint>{{.x = 50.0, .y = 50.0}}, 8.0);
    occupancy.addPolyline(line({{.x = 300.0, .y = 150.0}}), 2.0);
    EXPECT_GT(occupancy.coverage({.left = 40.0, .top = 40.0, .right = 60.0, .bottom = 60.0}), 0.0);
    EXPECT_GT(occupancy.coverage({.left = 290.0, .top = 140.0, .right = 310.0, .bottom = 160.0}),
              0.0);
    EXPECT_DOUBLE_EQ(
        occupancy.coverage({.left = 100.0, .top = 0.0, .right = 250.0, .bottom = 200.0}), 0.0);
}

TEST(Occupancy, Boxes)
{
    Occupancy occupancy;
    occupancy.reset(kArea);
    occupancy.addBox({.left = 100.0, .top = 40.0, .right = 200.0, .bottom = 80.0});
    EXPECT_DOUBLE_EQ(
        occupancy.coverage({.left = 100.0, .top = 40.0, .right = 200.0, .bottom = 80.0}), 1.0);
    EXPECT_DOUBLE_EQ(occupancy.coverage({.left = 0.0, .top = 0.0, .right = 96.0, .bottom = 200.0}),
                     0.0);
    occupancy.addBox({.left = 500.0, .top = 0.0, .right = 600.0, .bottom = 200.0});  // outside
    EXPECT_DOUBLE_EQ(
        occupancy.coverage({.left = 300.0, .top = 0.0, .right = 400.0, .bottom = 200.0}), 0.0);
}

TEST(Occupancy, IgnoresWhatIsOutside)
{
    Occupancy occupancy;
    occupancy.reset(kArea);
    occupancy.addPolyline(line({{.x = -100.0, .y = -50.0}, {.x = -10.0, .y = 300.0}}), 2.0);
    occupancy.addPoints(std::vector<PixelPoint>{{.x = 500.0, .y = 100.0}}, 8.0);
    EXPECT_DOUBLE_EQ(occupancy.coverage(kArea), 0.0);
}

}  // namespace
