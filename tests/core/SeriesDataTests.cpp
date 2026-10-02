#include "core/SeriesData.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace
{

using rocketplot::Range;
using rocketplot::UniformX;
using rocketplot::core::SeriesData;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(SeriesData, OwnedXY)
{
    SeriesData data;
    data.setOwned(std::vector<double>{1, 2, 3}, std::vector<double>{10, 30, 20});
    EXPECT_EQ(data.size(), 3U);
    EXPECT_FALSE(data.isView());
    EXPECT_FALSE(data.isUniform());
    EXPECT_TRUE(data.isSortedByX());
    EXPECT_DOUBLE_EQ(data.x(1), 2.0);
    EXPECT_DOUBLE_EQ(data.y(1), 30.0);
    EXPECT_EQ(data.xBounds(), (Range{.min = 1, .max = 3}));
    EXPECT_EQ(data.yBounds(), (Range{.min = 10, .max = 30}));
}

TEST(SeriesData, SizeMismatchThrows)
{
    SeriesData data;
    EXPECT_THROW(data.setOwned(std::vector<double>{1, 2}, std::vector<double>{1}),
                 std::invalid_argument);
    const std::vector<double> x{1, 2};
    const std::vector<double> y{1};
    EXPECT_THROW(data.setView(x, y), std::invalid_argument);
}

TEST(SeriesData, UniformX)
{
    SeriesData data;
    data.setOwned(UniformX{.start = 10.0, .step = 0.5}, std::vector<double>{1, 2, 3, 4});
    EXPECT_TRUE(data.isUniform());
    EXPECT_TRUE(data.isSortedByX());
    EXPECT_TRUE(data.xs().empty());
    EXPECT_DOUBLE_EQ(data.x(3), 11.5);
    EXPECT_EQ(data.xBounds(), (Range{.min = 10.0, .max = 11.5}));
}

TEST(SeriesData, UniformWithNegativeStepIsUnsorted)
{
    SeriesData data;
    data.setOwned(UniformX{.start = 0.0, .step = -1.0}, std::vector<double>{1, 2});
    EXPECT_FALSE(data.isSortedByX());
}

TEST(SeriesData, DetectsUnsortedAndNaNX)
{
    SeriesData data;
    data.setOwned(std::vector<double>{1, 3, 2}, std::vector<double>{0, 0, 0});
    EXPECT_FALSE(data.isSortedByX());
    data.setOwned(std::vector<double>{1, kNaN, 3}, std::vector<double>{0, 0, 0});
    EXPECT_FALSE(data.isSortedByX());
    data.setOwned(std::vector<double>{1, 1, 3}, std::vector<double>{0, 0, 0});  // ties are sorted
    EXPECT_TRUE(data.isSortedByX());
}

TEST(SeriesData, BoundsSkipNonFinitePoints)
{
    SeriesData data;
    data.setOwned(std::vector<double>{0, 1, 2, 3, 4}, std::vector<double>{5, kNaN, -kInf, 7, 6});
    EXPECT_EQ(data.xBounds(), (Range{.min = 0, .max = 4}));
    EXPECT_EQ(data.yBounds(), (Range{.min = 5, .max = 7}));

    data.setOwned(std::vector<double>{0, 1}, std::vector<double>{kNaN, kNaN});
    EXPECT_FALSE(data.xBounds().isValid());
    EXPECT_FALSE(data.yBounds().isValid());
}

TEST(SeriesData, AppendUpdatesEverything)
{
    SeriesData data;
    data.setOwned(std::vector<double>{0, 1}, std::vector<double>{5, 6});
    const std::vector<double> x{2, 3};
    const std::vector<double> y{-1, 9};
    data.append(x, y);
    EXPECT_EQ(data.size(), 4U);
    EXPECT_TRUE(data.isSortedByX());
    EXPECT_EQ(data.yBounds(), (Range{.min = -1, .max = 9}));

    const std::vector<double> back{1.5};
    const std::vector<double> value{0};
    data.append(back, value);
    EXPECT_FALSE(data.isSortedByX());
}

TEST(SeriesData, AppendSamplesToUniform)
{
    SeriesData data;
    data.setOwned(UniformX{.start = 0.0, .step = 2.0}, std::vector<double>{1});
    const std::vector<double> more{2, 3};
    data.append(more);
    EXPECT_EQ(data.size(), 3U);
    EXPECT_DOUBLE_EQ(data.x(2), 4.0);
    EXPECT_EQ(data.xBounds(), (Range{.min = 0, .max = 4}));
}

TEST(SeriesData, AppendRejectsTheWrongKind)
{
    SeriesData                data;
    const std::vector<double> values{1, 2};
    data.setView(values, values);
    EXPECT_THROW(data.append(values, values), std::logic_error);
    data.setOwned(UniformX{}, std::vector<double>{1});
    EXPECT_THROW(data.append(values, values), std::logic_error);
    data.setOwned(std::vector<double>{1}, std::vector<double>{1});
    EXPECT_THROW(data.append(values), std::logic_error);
    EXPECT_THROW(data.append(values, std::vector<double>{1}), std::invalid_argument);
}

TEST(SeriesData, ViewReadsInPlaceAndRefreshes)
{
    std::vector<double> x{0, 1, 2};
    std::vector<double> y{1, 2, 3};
    SeriesData          data;
    data.setView(x, y);
    EXPECT_TRUE(data.isView());
    EXPECT_EQ(data.ys().data(), y.data());
    y[1] = 100;
    data.refresh();
    EXPECT_DOUBLE_EQ(data.yBounds().max, 100.0);
}

TEST(SeriesData, BoundsSearch)
{
    SeriesData data;
    data.setOwned(std::vector<double>{0, 1, 1, 2, 5}, std::vector<double>{0, 0, 0, 0, 0});
    EXPECT_EQ(data.lowerBound(1.0), 1U);
    EXPECT_EQ(data.upperBound(1.0), 3U);
    EXPECT_EQ(data.lowerBound(-1.0), 0U);
    EXPECT_EQ(data.lowerBound(6.0), 5U);
    EXPECT_EQ(data.upperBound(5.0), 5U);
}

// The same 200-point grid as UniformX and as an x array.
struct Grids
{
    SeriesData          uniform;
    SeriesData          explicitX;
    std::vector<double> x;
};

Grids makeGrids()
{
    const UniformX            grid{.start = -3.0, .step = 0.1};
    const std::vector<double> y(200, 0.0);
    Grids                     grids;
    grids.x.resize(y.size());
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        grids.x[i] = grid.start + (static_cast<double>(i) * grid.step);
    }
    grids.uniform.setOwned(grid, y);
    grids.explicitX.setOwned(grids.x, y);
    return grids;
}

TEST(SeriesData, UniformBoundsSearchMatchesExplicitBetweenPoints)
{
    const Grids grids = makeGrids();
    for (int step = 0; step < 700; ++step)
    {
        const double value = -5.0 + (0.037 * step);
        EXPECT_EQ(grids.uniform.lowerBound(value), grids.explicitX.lowerBound(value)) << value;
        EXPECT_EQ(grids.uniform.upperBound(value), grids.explicitX.upperBound(value)) << value;
    }
}

TEST(SeriesData, UniformBoundsSearchMatchesExplicitOnPoints)
{
    const Grids grids = makeGrids();
    for (const double value : grids.x)
    {
        EXPECT_EQ(grids.uniform.lowerBound(value), grids.explicitX.lowerBound(value)) << value;
        EXPECT_EQ(grids.uniform.upperBound(value), grids.explicitX.upperBound(value)) << value;
    }
}

TEST(SeriesData, EmptyAndClear)
{
    SeriesData data;
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(data.lowerBound(0.0), 0U);
    data.setOwned(std::vector<double>{1}, std::vector<double>{1});
    data.clear();
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(data.yBounds().isValid());
}

}  // namespace
