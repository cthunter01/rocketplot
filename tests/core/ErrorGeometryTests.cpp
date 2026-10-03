#include "core/ErrorGeometry.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/ErrorData.h"
#include "core/LineBand.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::AxisMapping;
using rocketplot::core::collectErrorBars;
using rocketplot::core::ColumnGrid;
using rocketplot::core::decimateErrorBand;
using rocketplot::core::ErrorBar;
using rocketplot::core::ErrorData;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;
using rocketplot::core::Scale;
using rocketplot::core::SeriesData;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Values are their own pixels: x in [0, 100] over 100 pixels, y likewise (growing downward, so a
// bar's high end is its bottom).
const AxisMapping  kX(Range{.min = 0.0, .max = 100.0}, 0.0, 100.0);
const AxisMapping  kY(Range{.min = 0.0, .max = 100.0}, 0.0, 100.0);
constexpr PixelBox kBox{.left = 0.0, .top = 0.0, .right = 100.0, .bottom = 100.0};

struct Fixture
{
    SeriesData data;
    ErrorData  errors;
};

Fixture withYErrors(std::vector<double> x, std::vector<double> y, const std::vector<double>& minus,
                    const std::vector<double>& plus)
{
    Fixture fixture;
    fixture.data.setOwned(std::move(x), std::move(y));
    fixture.errors.setY(fixture.data, minus, plus);
    return fixture;
}

// One-pixel columns over the plot, and how far outside it a band may reach.
constexpr ColumnGrid kGrid{.left = 0.0, .width = 1.0, .count = 100};
constexpr double     kClipTop    = -16.0;
constexpr double     kClipBottom = 116.0;

// What the band covers at the center of pixel column @p column: the y extent of the outline's
// vertices there. Empty if no polygon covers the column.
Range coveredAt(const Polyline& band, std::size_t column)
{
    const double center = kGrid.center(column);
    Range        extent = Range::empty();
    for (std::size_t r = 0; r < band.runCount(); ++r)
    {
        for (const PixelPoint& point : band.run(r))
        {
            if (point.x == center)
            {
                extent = extent.including(point.y);
            }
        }
    }
    return extent;
}

Polyline bandOf(const Fixture& fixture)
{
    Polyline band;
    decimateErrorBand(fixture.data, fixture.errors, kX, kY, kGrid, kClipTop, kClipBottom, band);
    return band;
}

TEST(ErrorGeometry, BarsRunBetweenTheirEnds)
{
    Fixture                   fixture = withYErrors({10, 50}, {20, 60}, {5, 0}, {10, 3});
    const std::vector<double> xError{2, 4};
    fixture.errors.setX(fixture.data, xError, xError);
    std::vector<ErrorBar> bars;
    EXPECT_EQ(collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 1.0, true, bars), 2U);
    ASSERT_EQ(bars.size(), 2U);
    EXPECT_EQ(bars[0].center, (PixelPoint{.x = 10.0, .y = 20.0}));
    EXPECT_DOUBLE_EQ(bars[0].top, 15.0);
    EXPECT_DOUBLE_EQ(bars[0].bottom, 30.0);
    EXPECT_DOUBLE_EQ(bars[0].left, 8.0);
    EXPECT_DOUBLE_EQ(bars[0].right, 12.0);
    // No error below the second point: that end is the point itself.
    EXPECT_DOUBLE_EQ(bars[1].top, 60.0);
    EXPECT_DOUBLE_EQ(bars[1].bottom, 63.0);
}

TEST(ErrorGeometry, WithoutYOnlyTheXErrorsAreBars)
{
    Fixture               fixture = withYErrors({10, 50}, {20, 60}, {5, 5}, {5, 5});
    std::vector<ErrorBar> bars;
    collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 1.0, false, bars);
    EXPECT_TRUE(bars.empty());
    const std::vector<double> xError{2, 4};
    fixture.errors.setX(fixture.data, xError, xError);
    collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 1.0, false, bars);
    ASSERT_EQ(bars.size(), 2U);
    EXPECT_DOUBLE_EQ(bars[0].top, bars[0].bottom);
    EXPECT_DOUBLE_EQ(bars[1].left, 46.0);
}

TEST(ErrorGeometry, BarsAreCutAtTheBoxAndPointsWithoutErrorsHaveNone)
{
    // The first point is above the box and reaches into it; the second has no error; the third's
    // bar stays outside.
    const Fixture fixture = withYErrors({10, 50, 90}, {-50, 50, 500}, {0, 0, 10}, {80, 0, 10});
    std::vector<ErrorBar> bars;
    collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 1.0, true, bars);
    ASSERT_EQ(bars.size(), 1U);
    EXPECT_DOUBLE_EQ(bars[0].center.y, -50.0);
    EXPECT_DOUBLE_EQ(bars[0].top, 0.0);
    EXPECT_DOUBLE_EQ(bars[0].bottom, 30.0);
    EXPECT_DOUBLE_EQ(bars[0].left, bars[0].right);
}

TEST(ErrorGeometry, AnXErrorReachesInFromBesideTheBox)
{
    Fixture fixture;
    fixture.data.setOwned(std::vector<double>{-20, 50, 130}, std::vector<double>{50, 50, 50});
    const std::vector<double> minus{0, 1, 40};
    const std::vector<double> plus{30, 1, 0};
    fixture.errors.setX(fixture.data, minus, plus);
    std::vector<ErrorBar> bars;
    collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 1.0, true, bars);
    ASSERT_EQ(bars.size(), 3U);
    EXPECT_DOUBLE_EQ(bars[0].left, 0.0);
    EXPECT_DOUBLE_EQ(bars[0].right, 10.0);
    EXPECT_DOUBLE_EQ(bars[2].left, 90.0);
    EXPECT_DOUBLE_EQ(bars[2].right, 100.0);
}

TEST(ErrorGeometry, OneBarPerCell)
{
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i < 1000; ++i)
    {
        x.push_back(50.0 + (i * 0.001));
        y.push_back(50.0);
    }
    const std::vector<double> error(1000, 5.0);
    const Fixture             fixture = withYErrors(x, y, error, error);
    std::vector<ErrorBar>     bars;
    EXPECT_EQ(collectErrorBars(fixture.data, fixture.errors, kX, kY, kBox, 4.0, true, bars), 1000U);
    EXPECT_EQ(bars.size(), 1U);
}

TEST(ErrorGeometry, ALowEndALogAxisCannotShowIsAtTheEdge)
{
    // y from 1 to 1000 over 300 pixels, growing upward.
    const AxisMapping     logY(Range{.min = 1.0, .max = 1000.0}, 300.0, 0.0, Scale::LOG);
    const PixelBox        box{.left = 0.0, .top = 0.0, .right = 100.0, .bottom = 300.0};
    const Fixture         fixture = withYErrors({50}, {10}, {20}, {90});
    std::vector<ErrorBar> bars;
    collectErrorBars(fixture.data, fixture.errors, kX, logY, box, 1.0, true, bars);
    ASSERT_EQ(bars.size(), 1U);
    EXPECT_DOUBLE_EQ(bars[0].center.y, 200.0);
    EXPECT_DOUBLE_EQ(bars[0].top, 100.0);     // 100
    EXPECT_DOUBLE_EQ(bars[0].bottom, 300.0);  // -10: down to the edge
}

TEST(ErrorGeometry, BandRunsBetweenTheEndsOfThePointsBars)
{
    const Fixture fixture = withYErrors({10, 20, 30}, {50, 60, 50}, {5, 5, 5}, {10, 10, 10});
    Polyline      band;
    EXPECT_EQ(
        decimateErrorBand(fixture.data, fixture.errors, kX, kY, kGrid, kClipTop, kClipBottom, band),
        3U);
    // Half way between the first two points (the center of column 15 is at 15.5).
    const Range between = coveredAt(band, 15);
    EXPECT_NEAR(between.min, 50.5, 1e-9);
    EXPECT_NEAR(between.max, 65.5, 1e-9);
    // The band starts and ends with the points.
    EXPECT_FALSE(coveredAt(band, 8).isValid());
    EXPECT_FALSE(coveredAt(band, 31).isValid());
    EXPECT_TRUE(coveredAt(band, 10).isValid());
    EXPECT_TRUE(coveredAt(band, 29).isValid());
}

TEST(ErrorGeometry, BandIsMadeOfNarrowPieces)
{
    const Fixture  fixture = withYErrors({0, 100}, {50, 50}, {5, 5}, {5, 5});
    const Polyline band    = bandOf(fixture);
    EXPECT_GE(band.runCount(), 6U);  // 100 columns, 16 at most in each
    for (std::size_t r = 0; r < band.runCount(); ++r)
    {
        EXPECT_LE(band.run(r).size(), 2U * 18U);
    }
    for (std::size_t column = 0; column < kGrid.count; ++column)
    {
        EXPECT_EQ(coveredAt(band, column), (Range{.min = 45.0, .max = 55.0})) << column;
    }
}

TEST(ErrorGeometry, GapsSplitTheBandAndALonePointStillShows)
{
    const Fixture  fixture = withYErrors({10, 20, 30, 40, 50}, {50, 50, kNaN, 50, kNaN},
                                         {5, 5, 5, 5, 5}, {5, 5, 5, 5, 5});
    const Polyline band    = bandOf(fixture);
    EXPECT_TRUE(coveredAt(band, 15).isValid());
    EXPECT_FALSE(coveredAt(band, 25).isValid());
    EXPECT_FALSE(coveredAt(band, 35).isValid());
    // The point on its own at 40: a sliver a few pixels wide.
    EXPECT_EQ(coveredAt(band, 39), (Range{.min = 45.0, .max = 55.0}));
    EXPECT_EQ(coveredAt(band, 40), (Range{.min = 45.0, .max = 55.0}));
    EXPECT_FALSE(coveredAt(band, 44).isValid());
}

TEST(ErrorGeometry, DenseBandCoversEveryColumnsExtremes)
{
    // 100 points per pixel column, with errors that vary within each column.
    constexpr std::size_t kCount = 10000;
    std::vector<double>   x(kCount);
    std::vector<double>   y(kCount);
    std::vector<double>   error(kCount);
    for (std::size_t i = 0; i < kCount; ++i)
    {
        x[i]     = 100.0 * static_cast<double>(i) / kCount;
        y[i]     = 40.0 + (10.0 * static_cast<double>(i % 7));
        error[i] = static_cast<double>(i % 13);
    }
    const Fixture fixture = withYErrors(x, y, error, error);
    Polyline      band;
    EXPECT_EQ(
        decimateErrorBand(fixture.data, fixture.errors, kX, kY, kGrid, kClipTop, kClipBottom, band),
        kCount);
    // Column 42 holds points 4200 to 4299: the band there spans all their bars.
    Range expected = Range::empty();
    for (std::size_t i = 4200; i < 4300; ++i)
    {
        expected = expected.including(y[i] - error[i]).including(y[i] + error[i]);
    }
    EXPECT_EQ(coveredAt(band, 42), expected);
}

TEST(ErrorGeometry, BandIsCutOffOutsideThePlot)
{
    // Far above the plot on the left, far below it on the right.
    const Fixture  fixture = withYErrors({-1e6, 50, 1e6}, {-1e7, 50, 1e7}, {5, 5, 5}, {5, 5, 5});
    const Polyline band    = bandOf(fixture);
    Range          across  = Range::empty();
    Range          upright = Range::empty();
    for (std::size_t r = 0; r < band.runCount(); ++r)
    {
        for (const PixelPoint& point : band.run(r))
        {
            across  = across.including(point.x);
            upright = upright.including(point.y);
        }
    }
    EXPECT_EQ(across, (Range{.min = 0.0, .max = 100.0}));
    EXPECT_EQ(upright, (Range{.min = kClipTop, .max = kClipBottom}));
    // Its edges keep their slope: 10 pixels down per pixel across (all but), through (50, 45) and
    // (50, 55).
    const Range nearMiddle = coveredAt(band, 49);
    EXPECT_NEAR(nearMiddle.min, 40.0, 1e-3);
    EXPECT_NEAR(nearMiddle.max, 50.0, 1e-3);
    EXPECT_EQ(coveredAt(band, 10), (Range{.min = kClipTop, .max = kClipTop}));
}

TEST(ErrorGeometry, BandOnlyVisitsTheVisibleRange)
{
    constexpr std::size_t kCount = 100000;
    std::vector<double>   x(kCount);
    for (std::size_t i = 0; i < kCount; ++i)
    {
        x[i] = static_cast<double>(i);
    }
    const std::vector<double> y(kCount, 50.0);
    const std::vector<double> error(kCount, 1.0);
    const Fixture             fixture = withYErrors(x, y, error, error);
    Polyline                  band;
    EXPECT_EQ(
        decimateErrorBand(fixture.data, fixture.errors, kX, kY, kGrid, kClipTop, kClipBottom, band),
        102U);
}

TEST(ErrorGeometry, NoBandForUnsortedDataOrWithoutYErrors)
{
    const Fixture unsorted = withYErrors({30, 10, 20}, {50, 50, 50}, {5, 5, 5}, {5, 5, 5});
    Polyline      band;
    EXPECT_EQ(decimateErrorBand(unsorted.data, unsorted.errors, kX, kY, kGrid, kClipTop,
                                kClipBottom, band),
              0U);
    EXPECT_EQ(band.runCount(), 0U);

    Fixture none;
    none.data.setOwned(std::vector<double>{1, 2}, std::vector<double>{1, 2});
    EXPECT_EQ(decimateErrorBand(none.data, none.errors, kX, kY, kGrid, kClipTop, kClipBottom, band),
              0U);
}

}  // namespace
