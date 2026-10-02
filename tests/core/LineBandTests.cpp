#include "core/LineBand.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "core/Decimator.h"

namespace
{

using rocketplot::core::ColumnGrid;
using rocketplot::core::LineBand;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;

const ColumnGrid kGrid{.left = 0.0, .width = 1.0, .count = 100};

Polyline line(const std::vector<PixelPoint>& points)
{
    Polyline result;
    for (const PixelPoint& p : points)
    {
        result.add(p);
    }
    result.endRun();
    return result;
}

void expectSpan(const LineBand& band, std::size_t column, double top, double bottom)
{
    EXPECT_DOUBLE_EQ(band.spans()[column].top, top) << "column " << column;
    EXPECT_DOUBLE_EQ(band.spans()[column].bottom, bottom) << "column " << column;
}

// A piece's top edge runs left to right and its bottom edge right to left, back to its start.
bool isMonotonicOutline(std::span<const PixelPoint> outline)
{
    const std::size_t half = outline.size() / 2;
    for (std::size_t i = 1; i < outline.size(); ++i)
    {
        const bool wrongWay = i < half ? outline[i].x < outline[i - 1].x
                                       : (i > half && outline[i].x > outline[i - 1].x);
        if (wrongWay)
        {
            return false;
        }
    }
    return outline.front().x == outline.back().x;
}

// Two pieces meet at the same edge, at the same height: no seam.
bool piecesJoin(std::span<const PixelPoint> previous, std::span<const PixelPoint> next)
{
    const std::size_t half = previous.size() / 2;
    return next.front() == previous[half - 1] && next.back() == previous[half];
}

TEST(LineBand, HorizontalLineIsTwoHalfWidthsThick)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 10, .y = 50}, {.x = 90, .y = 50}}), kGrid, 1.0, polygons);
    EXPECT_EQ(polygons.runCount(), 6U);  // 82 columns in pieces of 16
    for (std::size_t column = 10; column < 90; ++column)
    {
        expectSpan(band, column, 49.0, 51.0);
    }
    // Round caps: half a pixel beyond the end, the cap is narrower; a pixel and a half beyond,
    // nothing.
    EXPECT_GT(band.spans()[9].top, 49.0);
    EXPECT_FALSE(band.spans()[9].empty());
    EXPECT_TRUE(band.spans()[8].empty());
}

TEST(LineBand, VerticalLineIsTwoColumnsWide)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 50.3, .y = 10}, {.x = 50.3, .y = 90}}), kGrid, 1.0, polygons);
    EXPECT_TRUE(band.spans()[48].empty());
    EXPECT_FALSE(band.spans()[49].empty());
    EXPECT_FALSE(band.spans()[50].empty());
    EXPECT_TRUE(band.spans()[51].empty());
    EXPECT_NEAR(band.spans()[50].top, 9.0 + (1.0 - std::sqrt(1.0 - 0.04)),
                1e-9);  // round cap, 0.2 px off axis
    EXPECT_NEAR(band.spans()[50].bottom, 91.0 - (1.0 - std::sqrt(1.0 - 0.04)), 1e-9);
}

TEST(LineBand, DiagonalKeepsItsPerpendicularWidth)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 10, .y = 10}, {.x = 90, .y = 90}}), kGrid, 1.0, polygons);
    // At 45 degrees a line 2 wide covers 2 * sqrt(2) vertically.
    const auto& span = band.spans()[50];
    EXPECT_NEAR(span.bottom - span.top, 2.0 * std::sqrt(2.0), 1e-9);
    EXPECT_NEAR((span.top + span.bottom) / 2.0, 50.5, 1e-9);
}

TEST(LineBand, ZigzagInOneColumnCoversItsExtremes)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 19.9, .y = 50},
                       {.x = 20.2, .y = 10},
                       {.x = 20.5, .y = 90},
                       {.x = 20.8, .y = 40},
                       {.x = 21.1, .y = 45}}),
                 kGrid, 1.0, polygons);
    EXPECT_NEAR(band.spans()[20].top, 9.0, 0.1);
    EXPECT_NEAR(band.spans()[20].bottom, 91.0, 0.1);
}

TEST(LineBand, ThinLinesCoverEveryColumnTheyCross)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 10.05, .y = 10}, {.x = 10.95, .y = 90}}), kGrid, 0.25, polygons);
    EXPECT_FALSE(band.spans()[10].empty());  // misses the column center, still drawn
}

TEST(LineBand, OnePolygonPerRun)
{
    Polyline input;
    input.add({.x = 10, .y = 10});
    input.add({.x = 20, .y = 10});
    input.endRun();
    input.add({.x = 40, .y = 10});
    input.add({.x = 50, .y = 20});
    input.endRun();
    input.add({.x = 70, .y = 70});  // a lone point becomes a dot
    input.endRun();
    LineBand band;
    Polyline polygons;
    band.outline(input, kGrid, 1.0, polygons);
    EXPECT_EQ(polygons.runCount(), 3U);
}

TEST(LineBand, PolygonsAreMonotonicAndJoinUp)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = 10, .y = 50}, {.x = 30, .y = 20}, {.x = 60, .y = 70}}), kGrid, 1.5,
                 polygons);
    ASSERT_GT(polygons.runCount(), 1U);  // 53 columns: several pieces
    for (std::size_t r = 0; r < polygons.runCount(); ++r)
    {
        EXPECT_TRUE(isMonotonicOutline(polygons.run(r))) << "piece " << r;
        EXPECT_TRUE(r == 0 || piecesJoin(polygons.run(r - 1), polygons.run(r))) << "piece " << r;
    }
}

TEST(LineBand, PointsOutsideTheGridAreClamped)
{
    LineBand band;
    Polyline polygons;
    band.outline(line({{.x = -500, .y = 50}, {.x = 500, .y = 50}}), kGrid, 1.0, polygons);
    EXPECT_EQ(polygons.runCount(), 7U);  // the whole grid: 100 columns
    EXPECT_DOUBLE_EQ(band.spans()[0].top, 49.0);
    EXPECT_DOUBLE_EQ(band.spans()[99].bottom, 51.0);
}

}  // namespace
