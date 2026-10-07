#include "core/NearestPoint.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace
{

using rocketplot::Range;
using rocketplot::UniformX;
using rocketplot::core::AxisMapping;
using rocketplot::core::NearestPoint;
using rocketplot::core::nearestPoint;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Scale;
using rocketplot::core::SeriesData;
using rocketplot::core::tracedPoint;

constexpr double kNaN       = std::numeric_limits<double>::quiet_NaN();
constexpr double kUnlimited = std::numeric_limits<double>::infinity();
// What a search that finds nothing is taken to have found: a point no test expects.
constexpr NearestPoint kNothing{.index = 999, .pixel = {.x = -1.0, .y = -1.0}, .distance = -1.0};

// A plot area of 100 x 100 pixels in which a value is drawn at the pixel of the same number.
const AxisMapping  kX(Range{.min = 0.0, .max = 100.0}, 0.0, 100.0);
const AxisMapping  kY(Range{.min = 0.0, .max = 100.0}, 0.0, 100.0);
constexpr PixelBox kBox{.left = 0.0, .top = 0.0, .right = 100.0, .bottom = 100.0};

SeriesData series(std::vector<double> x, std::vector<double> y)
{
    SeriesData data;
    data.setOwned(std::move(x), std::move(y));
    return data;
}

std::optional<std::size_t> indexOf(const std::optional<NearestPoint>& point)
{
    return point ? std::optional(point->index) : std::nullopt;
}

std::optional<std::size_t> nearestIndex(const SeriesData& data, PixelPoint position,
                                        double reach = kUnlimited)
{
    return indexOf(nearestPoint(data, kX, kY, kBox, position, reach));
}

std::optional<std::size_t> tracedIndex(const SeriesData& data, PixelPoint position)
{
    return indexOf(tracedPoint(data, kX, kY, kBox, position));
}

// The same answer the slow way: every point, one after the other.
std::optional<std::size_t> nearestByVisitingAll(const SeriesData& data, const AxisMapping& x,
                                                const AxisMapping& y, PixelPoint position,
                                                double reach, double halfColumn = kUnlimited)
{
    std::optional<std::size_t> nearest;
    double                     best = reach * reach;
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        const PixelPoint pixel{.x = x.toPixel(data.x(i)), .y = y.toPixel(data.y(i))};
        if (!kBox.contains(pixel) || std::abs(pixel.x - position.x) > halfColumn)
        {
            continue;
        }
        const double dx      = pixel.x - position.x;
        const double dy      = pixel.y - position.y;
        const double squared = (dx * dx) + (dy * dy);
        if (squared < best || (squared == best && !nearest))
        {
            best    = squared;
            nearest = i;
        }
    }
    return nearest;
}

// Many points per pixel column: x from 0 to 100, y jumping about between 10 and 90.
SeriesData noise(std::size_t count, unsigned seed)
{
    std::mt19937                           engine(seed);
    std::uniform_real_distribution<double> value(10.0, 90.0);
    std::vector<double>                    x(count);
    std::vector<double>                    y(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        x[i] = 100.0 * static_cast<double>(i) / static_cast<double>(count);
        y[i] = value(engine);
    }
    return series(std::move(x), std::move(y));
}

// The same number of points along a curve that keeps to a narrow band.
SeriesData wave(std::size_t count)
{
    std::vector<double> x(count);
    std::vector<double> y(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        x[i] = 100.0 * static_cast<double>(i) / static_cast<double>(count);
        y[i] = 50.0 + (30.0 * std::sin(x[i] / 7.0)) + (2.0 * std::sin(x[i] * 40.0));
    }
    return series(std::move(x), std::move(y));
}

// x from 0.5 to 20000 in even steps, y anywhere from 0.001 to 100, and now and then a value that
// a log axis can't show.
SeriesData decades(std::size_t count, unsigned seed)
{
    std::mt19937                           engine(seed);
    std::uniform_real_distribution<double> exponent(-3.0, 2.0);
    std::vector<double>                    x(count);
    std::vector<double>                    y(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        x[i] = 0.5 + (0.2 * static_cast<double>(i));
        y[i] = i % 7 == 0 ? -1.0 : std::pow(10.0, exponent(engine));
    }
    return series(std::move(x), std::move(y));
}

std::vector<PixelPoint> positions(std::size_t count, unsigned seed)
{
    std::mt19937                           engine(seed);
    std::uniform_real_distribution<double> pixel(-5.0, 105.0);
    std::vector<PixelPoint>                points(count);
    for (PixelPoint& point : points)
    {
        point = {.x = pixel(engine), .y = pixel(engine)};
    }
    return points;
}

// nearestPoint
// -----------------------------------------------------------------------------------------------------

TEST(NearestPoint, IsThePointDrawnNearestThePosition)
{
    const SeriesData data = series({10, 20, 30, 40}, {10, 50, 20, 80});

    const NearestPoint found =
        nearestPoint(data, kX, kY, kBox, {.x = 23.0, .y = 46.0}, kUnlimited).value_or(kNothing);

    EXPECT_EQ(found.index, 1U);
    EXPECT_EQ(found.pixel, (PixelPoint{.x = 20.0, .y = 50.0}));
    EXPECT_DOUBLE_EQ(found.distance, 5.0);
    // Near in x is not enough: the point at x = 30 is 26 pixels below.
    EXPECT_EQ(nearestIndex(data, {.x = 29.0, .y = 46.0}), 1U);
    EXPECT_EQ(nearestIndex(data, {.x = 29.0, .y = 30.0}), 2U);
}

TEST(NearestPoint, OnlyWithinReach)
{
    const SeriesData data = series({10, 20, 30}, {50, 50, 50});

    EXPECT_EQ(nearestIndex(data, {.x = 20.0, .y = 60.0}, 10.0),
              1U);  // exactly as far as it reaches
    EXPECT_FALSE(nearestIndex(data, {.x = 20.0, .y = 60.5}, 10.0));
    EXPECT_FALSE(nearestIndex(data, {.x = 20.0, .y = 50.0}, -1.0));
    EXPECT_FALSE(nearestIndex(data, {.x = 20.0, .y = 50.0}, kNaN));
}

TEST(NearestPoint, OfPointsEquallyNearItIsTheFirst)
{
    const SeriesData data = series({10, 20, 30}, {50, 50, 50});

    EXPECT_EQ(nearestIndex(data, {.x = 15.0, .y = 50.0}), 0U);
    EXPECT_EQ(nearestIndex(data, {.x = 25.0, .y = 50.0}), 1U);
}

TEST(NearestPoint, LeavesOutWhatIsNotDrawn)
{
    // A gap, a point outside the box and a hidden marker are all nearer than the last point.
    const SeriesData          data = series({10, 20, 30, 40}, {kNaN, 150, 50, 50});
    const std::vector<double> sizes{8.0, 8.0, 0.0, 8.0};

    const std::optional<NearestPoint> found =
        nearestPoint(data, kX, kY, kBox, {.x = 12.0, .y = 50.0}, kUnlimited, sizes);

    EXPECT_EQ(indexOf(found), 3U);
    EXPECT_EQ(nearestIndex(data, {.x = 12.0, .y = 50.0}), 2U);  // without sizes, none is hidden
}

TEST(NearestPoint, NothingWithoutPoints)
{
    EXPECT_FALSE(nearestIndex(SeriesData(), {.x = 50.0, .y = 50.0}));
    EXPECT_FALSE(nearestIndex(series({10, 20}, {kNaN, kNaN}), {.x = 50.0, .y = 50.0}));
    EXPECT_FALSE(nearestIndex(series({10, 20}, {50, 50}), {.x = kNaN, .y = 50.0}));
}

TEST(NearestPoint, WorksWithoutAnOrderInX)
{
    const SeriesData data = series({80, 20, 50, 20}, {50, 20, 90, 80});

    ASSERT_FALSE(data.isSortedByX());
    EXPECT_EQ(nearestIndex(data, {.x = 25.0, .y = 70.0}), 3U);
    EXPECT_EQ(nearestIndex(data, {.x = 70.0, .y = 55.0}), 0U);
    EXPECT_FALSE(nearestIndex(data, {.x = 50.0, .y = 50.0}, 20.0));
}

TEST(NearestPoint, WorksWithXThatIsNotStored)
{
    SeriesData data;
    data.setOwned(UniformX{.start = 10.0, .step = 20.0}, {50, 10, 90, 50});

    EXPECT_EQ(nearestIndex(data, {.x = 45.0, .y = 70.0}), 2U);
    EXPECT_EQ(nearestIndex(data, {.x = 45.0, .y = 20.0}), 1U);
}

TEST(NearestPoint, MeasuresInPixelsOnALogScale)
{
    // Decades 25 pixels apart: 400 is nearer to 1000 on the screen, though nearer to 100 in value.
    const AxisMapping logX(Range{.min = 1.0, .max = 10000.0}, 0.0, 100.0, Scale::LOG);
    const SeriesData  data = series({-5, 0, 10, 100, 1000}, {50, 50, 50, 50, 50});

    const std::optional<NearestPoint> found =
        nearestPoint(data, logX, kY, kBox, {.x = logX.toPixel(400.0), .y = 50.0}, kUnlimited);

    EXPECT_EQ(indexOf(found), 4U);
}

// A series of many points per pixel is searched through its min/max pyramid.

class NearestPointInDenseData : public testing::TestWithParam<double>
{ };

TEST_P(NearestPointInDenseData, IsTheOneVisitingEveryPointFinds)
{
    const double     reach  = GetParam();
    const SeriesData jumpy  = noise(60'000, 1);
    const SeriesData smooth = wave(60'000);

    for (const PixelPoint position : positions(100, 2))
    {
        EXPECT_EQ(nearestIndex(jumpy, position, reach),
                  nearestByVisitingAll(jumpy, kX, kY, position, reach))
            << "at " << position.x << ", " << position.y;
        EXPECT_EQ(nearestIndex(smooth, position, reach),
                  nearestByVisitingAll(smooth, kX, kY, position, reach))
            << "at " << position.x << ", " << position.y;
    }
}

INSTANTIATE_TEST_SUITE_P(Reaches, NearestPointInDenseData, testing::Values(0.5, 20.0, kUnlimited));

TEST(NearestPoint, InDenseDataOnLogAxesIsTheOneVisitingEveryPointFinds)
{
    const AxisMapping logX(Range{.min = 1.0, .max = 1e4}, 0.0, 100.0, Scale::LOG);
    const AxisMapping logY(Range{.min = 1e-2, .max = 1e2}, 100.0, 0.0, Scale::LOG);
    const SeriesData  data = decades(60'000, 3);

    for (const PixelPoint position : positions(100, 4))
    {
        EXPECT_EQ(indexOf(nearestPoint(data, logX, logY, kBox, position, 20.0)),
                  nearestByVisitingAll(data, logX, logY, position, 20.0))
            << "at " << position.x << ", " << position.y;
    }
}

// tracedPoint
// ------------------------------------------------------------------------------------------------------

TEST(TracedPoint, IsTheNearerInXOfThePointsEitherSide)
{
    const SeriesData data = series({10, 30, 50}, {10, 90, 10});

    // However far away in y: the position's x decides.
    EXPECT_EQ(tracedIndex(data, {.x = 19.0, .y = 90.0}), 0U);
    EXPECT_EQ(tracedIndex(data, {.x = 21.0, .y = 10.0}), 1U);
    EXPECT_EQ(tracedIndex(data, {.x = 20.0, .y = 50.0}), 0U);  // halfway: the earlier
    EXPECT_EQ(tracedIndex(data, {.x = 30.2, .y = 0.0}), 1U);
}

TEST(TracedPoint, TellsHowFarAwayThePointIs)
{
    const SeriesData data = series({10, 30, 50}, {10, 90, 10});

    const NearestPoint found =
        tracedPoint(data, kX, kY, kBox, {.x = 27.0, .y = 86.0}).value_or(kNothing);

    EXPECT_EQ(found.index, 1U);
    EXPECT_EQ(found.pixel, (PixelPoint{.x = 30.0, .y = 90.0}));
    EXPECT_DOUBLE_EQ(found.distance, 5.0);
}

TEST(TracedPoint, NothingBeforeTheFirstPointAndAfterTheLast)
{
    const SeriesData data = series({10, 30, 50}, {10, 90, 10});

    EXPECT_FALSE(tracedIndex(data, {.x = 5.0, .y = 10.0}));
    EXPECT_FALSE(tracedIndex(data, {.x = 55.0, .y = 10.0}));
    // Within the pixel column of an end, the series is still there.
    EXPECT_EQ(tracedIndex(data, {.x = 9.6, .y = 10.0}), 0U);
    EXPECT_EQ(tracedIndex(data, {.x = 50.4, .y = 10.0}), 2U);
}

TEST(TracedPoint, TakesTheOtherNeighborWhenTheNearerIsNotDrawn)
{
    const SeriesData gap     = series({10, 30, 50, 70}, {10, kNaN, kNaN, 10});
    const SeriesData outside = series({10, 30, 50}, {10, 150, 10});

    EXPECT_EQ(tracedIndex(gap, {.x = 28.0, .y = 50.0}), 0U);
    EXPECT_FALSE(tracedIndex(gap, {.x = 40.0, .y = 50.0}));  // inside the gap
    EXPECT_EQ(tracedIndex(gap, {.x = 52.0, .y = 50.0}), 3U);
    EXPECT_EQ(tracedIndex(outside, {.x = 28.0, .y = 50.0}), 0U);
}

TEST(TracedPoint, OfThePointsInThePixelColumnItIsTheNearest)
{
    // Five points within a pixel of x = 40, from y = 10 up to y = 90.
    const SeriesData data =
        series({20, 39.8, 39.9, 40.0, 40.1, 40.2, 60}, {50, 10, 30, 50, 70, 90, 50});

    EXPECT_EQ(tracedIndex(data, {.x = 40.0, .y = 12.0}), 1U);
    EXPECT_EQ(tracedIndex(data, {.x = 40.0, .y = 72.0}), 4U);
    EXPECT_EQ(tracedIndex(data, {.x = 40.0, .y = 100.0}), 5U);
}

TEST(TracedPoint, InDenseDataIsTheNearestOfThePixelColumn)
{
    const SeriesData jumpy = noise(60'000, 5);

    for (const PixelPoint position : positions(100, 6))
    {
        if (position.x < 1.0 || position.x > 99.0)
        {
            continue;  // at the ends of the data: see NothingBeforeTheFirstPointAndAfterTheLast
        }
        EXPECT_EQ(tracedIndex(jumpy, position),
                  nearestByVisitingAll(jumpy, kX, kY, position, kUnlimited, 0.5))
            << "at " << position.x << ", " << position.y;
    }
}

TEST(TracedPoint, WithoutAnOrderInXItIsTheNearestPointAnywhere)
{
    const SeriesData data = series({80, 20, 50, 20}, {50, 20, 90, 80});

    EXPECT_EQ(tracedIndex(data, {.x = 40.0, .y = 20.0}), 1U);
    EXPECT_EQ(tracedIndex(data, {.x = 60.0, .y = 40.0}), 0U);
}

TEST(TracedPoint, LeavesOutHiddenMarkers)
{
    const SeriesData          data = series({10, 30, 50}, {50, 50, 50});
    const std::vector<double> sizes{8.0, 0.0, 8.0};

    // Like a gap: the neighbor on the other side stands in for the hidden point.
    EXPECT_EQ(indexOf(tracedPoint(data, kX, kY, kBox, {.x = 22.0, .y = 50.0}, sizes)), 0U);
    EXPECT_EQ(indexOf(tracedPoint(data, kX, kY, kBox, {.x = 31.0, .y = 50.0}, sizes)), 2U);
    EXPECT_EQ(indexOf(tracedPoint(data, kX, kY, kBox, {.x = 31.0, .y = 50.0})), 1U);
}

}  // namespace
