#include "rocketplot/ScatterSeries.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QRgb>
#include <QSignalSpy>
#include <Qt>
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::ScatterSeries;

static_assert(rocketplot::ColorRange<std::vector<QColor>>);
static_assert(rocketplot::ColorRange<QList<QColor>>);
static_assert(rocketplot::ColorRange<std::array<Qt::GlobalColor, 2>>);
static_assert(rocketplot::ColorRange<std::vector<QRgb>>);
static_assert(!rocketplot::ColorRange<std::vector<double>>);  // values aren't colors
static_assert(!rocketplot::ColorRange<QColor>);

TEST(ScatterSeries, PointsUseTheSeriesSizeAndColorUnlessGivenTheirOwn)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    ScatterSeries* points = plot.addScatter(std::vector<double>{1, 2, 3});
    EXPECT_FALSE(points->hasSizes());
    EXPECT_FALSE(points->hasColors());
    EXPECT_DOUBLE_EQ(points->pointSize(1), points->markerSize());
    EXPECT_EQ(points->pointColor(1), points->color());

    const QSignalSpy changed(points, &rocketplot::Series::changed);
    points->setSizes(std::vector<int>{4, 12, 20});
    points->setColors(std::vector<QColor>{Qt::red, QColor(0, 255, 0, 128), Qt::blue});
    EXPECT_TRUE(points->hasSizes());
    EXPECT_TRUE(points->hasColors());
    EXPECT_DOUBLE_EQ(points->pointSize(1), 12.0);
    EXPECT_EQ(points->pointColor(0), QColor(Qt::red));
    EXPECT_EQ(points->pointColor(1), QColor(0, 255, 0, 128));
    EXPECT_EQ(changed.count(), 2);

    points->clearSizes();
    points->clearColors();
    EXPECT_FALSE(points->hasSizes());
    EXPECT_DOUBLE_EQ(points->pointSize(1), points->markerSize());
    EXPECT_EQ(points->pointColor(0), points->color());
    EXPECT_EQ(changed.count(), 4);
}

TEST(ScatterSeries, ColorsFromGlobalColorsAndRgb)
{
    PlotWidget     plot;
    ScatterSeries* points = plot.addScatter(std::vector<double>{1, 2});
    points->setColors(std::array<Qt::GlobalColor, 2>{Qt::red, Qt::darkCyan});
    EXPECT_EQ(points->pointColor(1), QColor(Qt::darkCyan));
    // A QRgb is opaque whatever its top byte, as in QColor's constructor.
    points->setColors(std::vector<QRgb>{0x2a78d6, 0xff00ff00});
    EXPECT_EQ(points->pointColor(0), QColor(0x2a, 0x78, 0xd6));
    EXPECT_EQ(points->pointColor(1), QColor(Qt::green));
}

TEST(ScatterSeries, SizesAndColorsNeedOneValuePerPoint)
{
    PlotWidget     plot;
    ScatterSeries* points = plot.addScatter(std::vector<double>{1, 2, 3});
    EXPECT_THROW(points->setSizes(std::vector<double>{1, 2}), std::invalid_argument);
    EXPECT_THROW(points->setSizes({1.0, 2.0, 3.0, 4.0}), std::invalid_argument);
    EXPECT_THROW(points->setColors(std::vector<QColor>{Qt::red}), std::invalid_argument);
    EXPECT_FALSE(points->hasSizes());
    EXPECT_FALSE(points->hasColors());
}

TEST(ScatterSeries, ASizeThatIsNotPositiveHidesItsPoint)
{
    PlotWidget     plot;
    ScatterSeries* points = plot.addScatter(std::vector<double>{1, 2, 3});
    points->setSizes({-4.0, std::numeric_limits<double>::quiet_NaN(), 6.0});
    EXPECT_DOUBLE_EQ(points->pointSize(0), 0.0);
    EXPECT_DOUBLE_EQ(points->pointSize(1), 0.0);
    EXPECT_DOUBLE_EQ(points->pointSize(2), 6.0);
}

TEST(ScatterSeries, SizesAndColorsGoWithThePointsTheyWereSetFor)
{
    PlotWidget     plot;
    ScatterSeries* points = plot.addScatter(std::vector<double>{0, 1}, std::vector<double>{0, 1});
    points->setSizes({10.0, 20.0});
    points->setColors(std::vector<QColor>{Qt::red, Qt::blue});
    points->append(2.0, 2.0);  // a point added later looks like the series
    EXPECT_DOUBLE_EQ(points->pointSize(1), 20.0);
    EXPECT_DOUBLE_EQ(points->pointSize(2), points->markerSize());
    EXPECT_EQ(points->pointColor(2), points->color());
    points->setData(std::vector<double>{5, 6}, std::vector<double>{5, 6});
    EXPECT_FALSE(points->hasSizes());
    EXPECT_FALSE(points->hasColors());
}

// Whether two colors are the same but for antialiasing at a marker's edge.
bool isNearly(const QColor& a, const QColor& b)
{
    constexpr int kTolerance = 12;
    return std::abs(a.red() - b.red()) <= kTolerance &&
           std::abs(a.green() - b.green()) <= kTolerance &&
           std::abs(a.blue() - b.blue()) <= kTolerance;
}

// Three points along the middle of the plot.
class ScatterSeriesTest : public rocketplot::test::RenderedPlotTest
{
protected:
    void SetUp() override
    {
        RenderedPlotTest::SetUp();
        m_points = m_plot.addScatter(std::vector<double>{2, 5, 8}, std::vector<double>{5, 5, 5});
    }

    ScatterSeries* m_points = nullptr;
};

TEST_F(ScatterSeriesTest, EachPointIsDrawnInItsColor)
{
    m_points->setColors(std::vector<QColor>{Qt::red, Qt::green, Qt::blue});
    const QImage image = render();
    EXPECT_EQ(image.pixelColor(pixelAt(2.0, 5.0)), QColor(Qt::red));
    EXPECT_EQ(image.pixelColor(pixelAt(5.0, 5.0)), QColor(Qt::green));
    EXPECT_EQ(image.pixelColor(pixelAt(8.0, 5.0)), QColor(Qt::blue));
    m_points->clearColors();
    EXPECT_EQ(render().pixelColor(pixelAt(5.0, 5.0)), m_points->color());
}

TEST_F(ScatterSeriesTest, EachPointIsDrawnAtItsSize)
{
    const QImage same = render();
    // 30 pixels across, 4 pixels across, and hidden.
    m_points->setSizes({30.0, 4.0, 0.0});
    const QImage sized = render();
    const QColor color = m_points->color();
    EXPECT_EQ(sized.pixelColor(pixelAt(2.0, 5.0, QPoint(12, 0))), color);
    EXPECT_NE(same.pixelColor(pixelAt(2.0, 5.0, QPoint(12, 0))), color);
    // The small one covers its middle, but not the pixels the default size does.
    EXPECT_TRUE(isNearly(sized.pixelColor(pixelAt(5.0, 5.0)), color));
    EXPECT_FALSE(isNearly(sized.pixelColor(pixelAt(5.0, 5.0, QPoint(3, 0))), color));
    EXPECT_TRUE(isNearly(same.pixelColor(pixelAt(5.0, 5.0, QPoint(2, 0))), color));
    EXPECT_FALSE(isNearly(sized.pixelColor(pixelAt(8.0, 5.0)), color));
    EXPECT_EQ(same.pixelColor(pixelAt(8.0, 5.0)), color);
}

TEST_F(ScatterSeriesTest, AMarkerTooBigForASpriteIsStillDrawn)
{
    m_points->setSizes({200.0, 8.0, 8.0});
    m_points->setColors(std::vector<QColor>{Qt::red, Qt::green, Qt::blue});
    EXPECT_EQ(render().pixelColor(pixelAt(2.0, 5.0, QPoint(0, 90))), QColor(Qt::red));
}

TEST_F(ScatterSeriesTest, ManySizesAndColorsRender)
{
    std::vector<double> x;
    std::vector<double> sizes;
    std::vector<QRgb>   colors;
    for (int i = 0; i < 20000; ++i)
    {
        x.push_back((i % 200) / 20.0);
        sizes.push_back(1.0 + (i % 37));
        colors.push_back(qRgb(i % 256, (i / 7) % 256, (i / 3) % 256));
    }
    ScatterSeries* cloud = m_plot.addScatter(x, x);
    cloud->setSizes(sizes);
    cloud->setColors(colors);
    m_plot.setDebugOverlay(true);
    EXPECT_FALSE(m_plot.grab().isNull());
}

}  // namespace
