#include "core/ErrorData.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::ErrorData;
using rocketplot::core::SeriesData;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

SeriesData points(std::vector<double> x, std::vector<double> y)
{
    SeriesData data;
    data.setOwned(std::move(x), std::move(y));
    return data;
}

TEST(ErrorData, EmptyUntilSet)
{
    const ErrorData errors;
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(errors.hasX());
    EXPECT_FALSE(errors.hasY());
    EXPECT_FALSE(errors.yBounds().isValid());
}

TEST(ErrorData, EndsAreValuesAroundEachPoint)
{
    const SeriesData          data = points({1, 2, 3}, {10, 20, 30});
    ErrorData                 errors;
    const std::vector<double> minus{1, 2, 3};
    const std::vector<double> plus{4, 5, 6};
    errors.setY(data, minus, plus);
    ASSERT_TRUE(errors.hasY());
    EXPECT_FALSE(errors.hasX());
    EXPECT_DOUBLE_EQ(errors.yLow()[1], 18.0);
    EXPECT_DOUBLE_EQ(errors.yHigh()[1], 25.0);
    EXPECT_EQ(errors.yBounds(), (Range{.min = 9.0, .max = 36.0}));

    const std::vector<double> half{0.5, 0.5, 0.5};
    errors.setX(data, half, half);
    EXPECT_TRUE(errors.hasX());
    EXPECT_EQ(errors.xBounds(), (Range{.min = 0.5, .max = 3.5}));
    EXPECT_DOUBLE_EQ(errors.xReachBelow(), 0.5);
    EXPECT_DOUBLE_EQ(errors.xReachAbove(), 0.5);

    errors.clear();
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(errors.xBounds().isValid());
}

TEST(ErrorData, SignsAreIgnoredAndBadErrorsAreNone)
{
    const SeriesData          data = points({1, 2, 3}, {10, 20, 30});
    ErrorData                 errors;
    const std::vector<double> minus{-1, kNaN, std::numeric_limits<double>::infinity()};
    const std::vector<double> plus{-2, 2, kNaN};
    errors.setY(data, minus, plus);
    EXPECT_DOUBLE_EQ(errors.yLow()[0], 9.0);
    EXPECT_DOUBLE_EQ(errors.yHigh()[0], 12.0);
    EXPECT_DOUBLE_EQ(errors.yLow()[1], 20.0);
    EXPECT_DOUBLE_EQ(errors.yHigh()[1], 22.0);
    EXPECT_DOUBLE_EQ(errors.yLow()[2], 30.0);
    EXPECT_DOUBLE_EQ(errors.yHigh()[2], 30.0);
}

TEST(ErrorData, GapsHaveNoBars)
{
    const SeriesData          data = points({1, kNaN, 3}, {10, 20, kNaN});
    ErrorData                 errors;
    const std::vector<double> error{100, 100, 100};
    errors.setY(data, error, error);
    EXPECT_TRUE(std::isnan(errors.yLow()[1]));
    EXPECT_TRUE(std::isnan(errors.yHigh()[2]));
    EXPECT_EQ(errors.yBounds(), (Range{.min = -90.0, .max = 110.0}));
}

TEST(ErrorData, WrongSizeThrows)
{
    const SeriesData          data = points({1, 2, 3}, {10, 20, 30});
    ErrorData                 errors;
    const std::vector<double> two{1, 1};
    const std::vector<double> three{1, 1, 1};
    EXPECT_THROW(errors.setY(data, two, three), std::invalid_argument);
    EXPECT_THROW(errors.setX(data, three, two), std::invalid_argument);
    EXPECT_TRUE(errors.empty());
}

TEST(ErrorData, PositiveBoundsAreWhatALogAxisShows)
{
    const SeriesData          data = points({1, 2, 3}, {-5, 2, 10});
    ErrorData                 errors;
    const std::vector<double> minus{1, 3, 4};
    const std::vector<double> plus{20, 1, 5};
    errors.setY(data, minus, plus);
    // -5 isn't shown at all; 2 reaches below zero, so only its high end counts.
    EXPECT_EQ(errors.yPositiveBounds(), (Range{.min = 3.0, .max = 15.0}));
}

TEST(ErrorData, BoundsWithinAnXRange)
{
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> error;
    for (int i = 0; i < 1000; ++i)
    {
        x.push_back(i);
        y.push_back(i);
        error.push_back(i < 500 ? 1.0 : 100.0);
    }
    const SeriesData data = points(x, y);
    ErrorData        errors;
    errors.setY(data, error, error);
    EXPECT_EQ(errors.yBoundsWithin(data, {.min = 100.0, .max = 200.0}, false),
              (Range{.min = 99.0, .max = 201.0}));
    EXPECT_EQ(errors.yBoundsWithin(data, {.min = 600.0, .max = 700.0}, false),
              (Range{.min = 500.0, .max = 800.0}));
    EXPECT_EQ(errors.yBoundsWithin(data, {.min = 0.0, .max = 10.0}, true),
              (Range{.min = 1.0, .max = 11.0}));
    EXPECT_FALSE(errors.yBoundsWithin(data, {.min = 2000.0, .max = 3000.0}, false).isValid());
}

TEST(ErrorData, BoundsWithinAnXRangeOfUnsortedData)
{
    const SeriesData          data = points({5, 1, 3}, {50, 10, 30});
    ErrorData                 errors;
    const std::vector<double> error{1, 2, 3};
    errors.setY(data, error, error);
    EXPECT_EQ(errors.yBoundsWithin(data, {.min = 0.0, .max = 4.0}, false),
              (Range{.min = 8.0, .max = 33.0}));
}

TEST(ErrorData, PointsAppendedLaterHaveNone)
{
    SeriesData                data = points({1, 2}, {10, 20});
    ErrorData                 errors;
    const std::vector<double> error{1, 1};
    errors.setY(data, error, error);
    const std::vector<double> moreX{3};
    const std::vector<double> moreY{1000};
    data.append(moreX, moreY);
    EXPECT_EQ(errors.yLow().size(), 2U);
    EXPECT_EQ(errors.yBoundsWithin(data, {.min = 0.0, .max = 10.0}, false),
              (Range{.min = 9.0, .max = 21.0}));
}

}  // namespace
