#include "core/Interpolate.h"

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "core/SeriesData.h"
#include "rocketplot/UniformX.h"

namespace
{

using rocketplot::core::interpolateY;
using rocketplot::core::Scale;
using rocketplot::core::SeriesData;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

SeriesData dataOf(std::vector<double> x, std::vector<double> y)
{
    SeriesData data;
    data.setOwned(std::move(x), std::move(y));
    return data;
}

TEST(Interpolate, LinearBetweenTheTwoPointsAround)
{
    const SeriesData data = dataOf({0, 10, 20}, {0, 100, 50});
    EXPECT_DOUBLE_EQ(interpolateY(data, 5.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 50.0);
    EXPECT_DOUBLE_EQ(interpolateY(data, 15.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 75.0);
    // At a point, and at either end.
    EXPECT_DOUBLE_EQ(interpolateY(data, 10.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 100.0);
    EXPECT_DOUBLE_EQ(interpolateY(data, 0.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 0.0);
    EXPECT_DOUBLE_EQ(interpolateY(data, 20.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 50.0);
}

TEST(Interpolate, UniformX)
{
    SeriesData data;
    data.setOwned(rocketplot::UniformX{.start = 1.0, .step = 0.5}, std::vector<double>{2, 4, 8});
    EXPECT_DOUBLE_EQ(interpolateY(data, 1.25, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 3.0);
}

TEST(Interpolate, NothingOutsideTheDataOrForAPath)
{
    const SeriesData data = dataOf({0, 10}, {0, 100});
    EXPECT_FALSE(interpolateY(data, -0.1, Scale::LINEAR, Scale::LINEAR));
    EXPECT_FALSE(interpolateY(data, 10.1, Scale::LINEAR, Scale::LINEAR));
    EXPECT_FALSE(interpolateY(data, kNaN, Scale::LINEAR, Scale::LINEAR));
    EXPECT_FALSE(interpolateY(dataOf({}, {}), 0.0, Scale::LINEAR, Scale::LINEAR));
    // x goes up and back: no single y at an x.
    EXPECT_FALSE(interpolateY(dataOf({0, 10, 5}, {0, 1, 2}), 5.0, Scale::LINEAR, Scale::LINEAR));
}

TEST(Interpolate, NothingInAGap)
{
    const SeriesData data = dataOf({0, 10, 20, 30}, {0, kNaN, 50, 60});
    EXPECT_FALSE(interpolateY(data, 5.0, Scale::LINEAR, Scale::LINEAR));
    EXPECT_FALSE(interpolateY(data, 10.0, Scale::LINEAR, Scale::LINEAR));
    EXPECT_FALSE(interpolateY(data, 15.0, Scale::LINEAR, Scale::LINEAR));
    EXPECT_DOUBLE_EQ(interpolateY(data, 25.0, Scale::LINEAR, Scale::LINEAR).value_or(kNaN), 55.0);
}

TEST(Interpolate, InTheAxesScales)
{
    // Halfway between 1 and 100 on a log y axis is 10, where the drawn segment is.
    const SeriesData data = dataOf({0, 10}, {1, 100});
    EXPECT_DOUBLE_EQ(interpolateY(data, 5.0, Scale::LINEAR, Scale::LOG).value_or(kNaN), 10.0);
    // On a log x axis, 10 is halfway between 1 and 100.
    const SeriesData logX = dataOf({1, 100}, {0, 2});
    EXPECT_DOUBLE_EQ(interpolateY(logX, 10.0, Scale::LOG, Scale::LINEAR).value_or(kNaN), 1.0);
    // A value a log axis can't show is a gap.
    EXPECT_FALSE(interpolateY(dataOf({0, 10}, {-1, 100}), 5.0, Scale::LINEAR, Scale::LOG));
}

}  // namespace
