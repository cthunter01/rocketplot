#include "core/Decimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace
{

using rocketplot::Range;
using rocketplot::UniformX;
using rocketplot::core::AxisMapping;
using rocketplot::core::decimateLine;
using rocketplot::core::decimateScatter;
using rocketplot::core::DecimationMode;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;
using rocketplot::core::SeriesData;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// 100 pixel columns over x in [0, 1000); y maps value v to pixel v (pixelStart = min).
const AxisMapping kX(Range{.min = 0.0, .max = 1000.0}, 0.0, 100.0);
const AxisMapping kY(Range{.min = -1000.0, .max = 1000.0}, -1000.0, 1000.0);

std::vector<double> randomWalk(std::size_t count, unsigned seed)
{
    std::mt19937                     engine(seed);
    std::normal_distribution<double> step(0.0, 1.0);
    std::vector<double>              y(count);
    double                           value = 0.0;
    for (double& v : y)
    {
        value += step(engine);
        v = value;
    }
    return y;
}

std::vector<double> evenX(std::size_t count, double span)
{
    std::vector<double> x(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        x[i] = span * static_cast<double>(i) / static_cast<double>(count);
    }
    return x;
}

std::vector<PixelPoint> allPoints(const Polyline& line)
{
    std::vector<PixelPoint> points;
    for (std::size_t r = 0; r < line.runCount(); ++r)
    {
        const auto run = line.run(r);
        points.insert(points.end(), run.begin(), run.end());
    }
    return points;
}

// Per pixel column: the lowest and highest y pixel.
std::map<int, std::pair<double, double>> columnExtents(const std::vector<PixelPoint>& points)
{
    std::map<int, std::pair<double, double>> extents;
    for (const PixelPoint& p : points)
    {
        const int column = static_cast<int>(std::floor(p.x));
        auto [it, added] = extents.try_emplace(column, p.y, p.y);
        if (!added)
        {
            it->second.first  = std::min(it->second.first, p.y);
            it->second.second = std::max(it->second.second, p.y);
        }
    }
    return extents;
}

TEST(Decimator, FewPointsAreDrawnAsTheyAre)
{
    SeriesData data;
    data.setOwned(std::vector<double>{100, 200, 300}, std::vector<double>{1, 5, 2});
    Polyline   line;
    const auto result = decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(result.mode, DecimationMode::RAW);
    ASSERT_EQ(line.runCount(), 1U);
    EXPECT_EQ(line.pointCount(), 3U);
    EXPECT_EQ(line.run(0)[1], (PixelPoint{.x = 20.0, .y = 5.0}));
}

TEST(Decimator, MinMaxKeepsEveryColumnsExtremes)
{
    const std::size_t n = 200'000;
    SeriesData        data;
    data.setOwned(evenX(n, 1000.0), randomWalk(n, 7));
    Polyline   line;
    const auto result = decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(result.mode, DecimationMode::MIN_MAX);
    EXPECT_LE(line.pointCount(), 4U * 101U);
    EXPECT_EQ(line.runCount(), 1U);

    std::vector<PixelPoint> every;
    every.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        every.push_back({.x = kX.toPixel(data.x(i)), .y = kY.toPixel(data.y(i))});
    }
    EXPECT_EQ(columnExtents(allPoints(line)), columnExtents(every));
}

TEST(Decimator, OutputPointsAreDataPointsInOrder)
{
    const std::size_t n = 50'000;
    SeriesData        data;
    data.setOwned(evenX(n, 1000.0), randomWalk(n, 11));
    Polyline line;
    decimateLine(data, kX, kY, 1.0, line);
    const auto points = allPoints(line);
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        EXPECT_GE(points[i].x, points[i - 1].x);  // x never goes back: points are in index order
    }
}

TEST(Decimator, UniformXMatchesExplicitX)
{
    const std::size_t n = 100'000;
    const auto        y = randomWalk(n, 13);
    SeriesData        explicitX;
    SeriesData        uniform;
    explicitX.setOwned(evenX(n, 1000.0), y);
    uniform.setOwned(UniformX{.start = 0.0, .step = 1000.0 / static_cast<double>(n)}, y);
    Polyline a;
    Polyline b;
    decimateLine(explicitX, kX, kY, 1.0, a);
    decimateLine(uniform, kX, kY, 1.0, b);
    EXPECT_EQ(columnExtents(allPoints(a)), columnExtents(allPoints(b)));
    EXPECT_EQ(a.runCount(), b.runCount());
}

TEST(Decimator, NaNRunsBreakTheLine)
{
    const std::size_t n = 100'000;
    auto              y = randomWalk(n, 17);
    // Two dropouts, each several columns wide.
    std::fill(y.begin() + 20'000, y.begin() + 30'000, kNaN);
    std::fill(y.begin() + 60'000, y.begin() + 61'000, kNaN);
    SeriesData data;
    data.setOwned(evenX(n, 1000.0), y);
    Polyline line;
    decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(line.runCount(), 3U);
}

TEST(Decimator, NaNBreaksARawLine)
{
    SeriesData data;
    data.setOwned(std::vector<double>{100, 200, 300, 400, 500},
                  std::vector<double>{1, 2, kNaN, 4, 5});
    Polyline line;
    decimateLine(data, kX, kY, 1.0, line);
    ASSERT_EQ(line.runCount(), 2U);
    EXPECT_EQ(line.run(0).size(), 2U);
    EXPECT_EQ(line.run(1).size(), 2U);
}

TEST(Decimator, OnlyTheVisibleRangeIsVisited)
{
    const std::size_t n = 1'000'000;
    SeriesData        data;
    data.setOwned(UniformX{.start = 0.0, .step = 1.0}, randomWalk(n, 19));
    const AxisMapping zoomed(Range{.min = 500'000.0, .max = 500'100.0}, 0.0, 100.0);
    Polyline          line;
    const auto        result = decimateLine(data, zoomed, kY, 1.0, line);
    EXPECT_EQ(result.mode, DecimationMode::RAW);
    EXPECT_LE(result.visiblePoints, 103U);  // 101 visible plus one either side
    // The neighbors outside make the line run to both edges.
    EXPECT_LT(line.run(0).front().x, 0.0);
    EXPECT_GT(line.run(0).back().x, 100.0);
}

TEST(Decimator, UnsortedSkipsRepeatedPixels)
{
    // An ellipse (80 x 100 px) traced 8 times: consecutive points are mostly in the same pixel.
    const std::size_t   n = 100'000;
    std::vector<double> x(n);
    std::vector<double> y(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) * 0.0005;
        x[i]           = 500.0 + (400.0 * std::cos(t));
        y[i]           = 50.0 * std::sin(t);
    }
    SeriesData data;
    data.setOwned(x, y);
    ASSERT_FALSE(data.isSortedByX());
    Polyline   line;
    const auto result = decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(result.mode, DecimationMode::PIXEL_SKIP);
    EXPECT_LT(line.pointCount(), n / 10);
    EXPECT_GT(line.pointCount(), 100U);
}

TEST(Decimator, EmptySeries)
{
    SeriesData data;
    Polyline   line;
    decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(line.runCount(), 0U);
}

TEST(Decimator, AllNaN)
{
    SeriesData data;
    data.setOwned(evenX(10'000, 1000.0), std::vector<double>(10'000, kNaN));
    Polyline line;
    decimateLine(data, kX, kY, 1.0, line);
    EXPECT_EQ(line.pointCount(), 0U);
}

TEST(Decimator, ScatterKeepsOnePointPerCell)
{
    // 10,000 points on a 10 x 10 grid of cells: at most 100 survive, all inside the box.
    std::vector<double> x;
    std::vector<double> y;
    std::mt19937 engine(23);  // NOLINT(bugprone-random-generator-seed): reproducible test data
    std::uniform_real_distribution<double> value(0.0, 1000.0);
    for (int i = 0; i < 10'000; ++i)
    {
        x.push_back(value(engine));
        y.push_back(value(engine) - 500.0);
    }
    SeriesData data;
    data.setOwned(x, y);
    const PixelBox          box{.left = 0.0, .top = -500.0, .right = 100.0, .bottom = 500.0};
    std::vector<PixelPoint> points;
    const std::size_t       visited = decimateScatter(data, kX, kY, box, 10.0, points);
    EXPECT_EQ(visited, 10'000U);
    EXPECT_LE(points.size(), 11U * 101U);
    EXPECT_GT(points.size(), 50U);
    for (const PixelPoint& p : points)
    {
        EXPECT_TRUE(box.contains(p));
    }
}

TEST(Decimator, ScatterReportsWhichPointsItKept)
{
    SeriesData data;
    data.setOwned(std::vector<double>{100, 100.5, 500, 5000, 900},
                  std::vector<double>{0, 0.5, 10, 0, -20});
    const PixelBox           box{.left = 0.0, .top = -500.0, .right = 100.0, .bottom = 500.0};
    std::vector<PixelPoint>  points;
    std::vector<std::size_t> indices{99};
    decimateScatter(data, kX, kY, box, 5.0, points, &indices);
    // The second shares the first's cell; the fourth is outside the box.
    EXPECT_EQ(indices, (std::vector<std::size_t>{0, 2, 4}));
    ASSERT_EQ(points.size(), 3U);
    EXPECT_DOUBLE_EQ(points[1].x, 50.0);
}

TEST(Decimator, ScatterLeavesOutHiddenPoints)
{
    SeriesData data;
    data.setOwned(std::vector<double>{100, 100.5, 500, 900}, std::vector<double>{0, 0.5, 10, -20});
    const PixelBox            box{.left = 0.0, .top = -500.0, .right = 100.0, .bottom = 500.0};
    std::vector<PixelPoint>   points;
    std::vector<std::size_t>  indices;
    const std::vector<double> sizes{0.0, 8.0, kNaN};  // none for the last point: it is kept
    decimateScatter(data, kX, kY, box, 5.0, points, &indices, sizes);
    // The hidden first point doesn't take the cell from the second.
    EXPECT_EQ(indices, (std::vector<std::size_t>{1, 3}));
}

TEST(Decimator, ScatterOnSortedDataOnlyVisitsTheBox)
{
    SeriesData data;
    data.setOwned(UniformX{.start = 0.0, .step = 1.0}, std::vector<double>(100'000, 0.0));
    const PixelBox          box{.left = 0.0, .top = -10.0, .right = 100.0, .bottom = 10.0};
    std::vector<PixelPoint> points;
    const std::size_t       visited = decimateScatter(data, kX, kY, box, 1.0, points);
    EXPECT_LE(visited, 1001U);
}

TEST(Decimator, LogXColumnsMatchBruteForce)
{
    using rocketplot::core::Scale;
    // x from 1 to 10^5 (sorted), columns spaced evenly in log10(x).
    const std::size_t   n = 200'000;
    std::vector<double> x(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        x[i] = std::pow(10.0, 5.0 * static_cast<double>(i) / static_cast<double>(n));
    }
    SeriesData data;
    data.setOwned(x, randomWalk(n, 29));
    const AxisMapping logX(Range{.min = 1.0, .max = 1e5}, 0.0, 100.0, Scale::LOG);
    Polyline          line;
    const auto        result = decimateLine(data, logX, kY, 1.0, line);
    EXPECT_EQ(result.mode, DecimationMode::MIN_MAX);

    std::vector<PixelPoint> every;
    every.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        every.push_back({.x = logX.toPixel(data.x(i)), .y = kY.toPixel(data.y(i))});
    }
    EXPECT_EQ(columnExtents(allPoints(line)), columnExtents(every));
}

TEST(Decimator, NonPositiveValuesOnALogAxisAreGaps)
{
    using rocketplot::core::Scale;
    SeriesData data;
    data.setOwned(std::vector<double>{1, 2, 3, 4, 5}, std::vector<double>{10, 100, -5, 100, 10});
    const AxisMapping logY(Range{.min = 1.0, .max = 1000.0}, 300.0, 0.0, Scale::LOG);
    Polyline          line;
    decimateLine(data, kX, logY, 1.0, line);
    EXPECT_EQ(line.runCount(), 2U);
}

}  // namespace
