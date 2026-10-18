#include "rocketplot/BarSeries.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QTest>
#include <Qt>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::BarSeries;
using rocketplot::PlotWidget;
using rocketplot::Range;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Three bars: 2 from 1 to 3, 6 from 3 to 5, 4 from 5 to 8.
BarSeries* addThree(PlotWidget& plot, const QString& name = {})
{
    return plot.addBars(std::vector{1.0, 3.0, 5.0, 8.0}, std::vector{2.0, 6.0, 4.0}, name);
}

TEST(BarSeries, TheEdgesMustBeFiniteAndIncreasing)
{
    PlotWidget plot;
    EXPECT_THROW(plot.addBars(std::vector{0.0, 1.0}, std::vector{1.0, 2.0}), std::invalid_argument);
    EXPECT_THROW(plot.addBars(std::vector{0.0, 1.0, 1.0}, std::vector{1.0, 2.0}),
                 std::invalid_argument);
    EXPECT_THROW(plot.addBars(std::vector{0.0, kNaN}, std::vector{1.0}), std::invalid_argument);
    EXPECT_TRUE(plot.addBars(std::vector<double>{}, std::vector<double>{})->isEmpty());
    EXPECT_EQ(plot.series().size(), 1);
}

TEST(BarSeries, ItsPointsAreTheBars)
{
    PlotWidget       plot;
    const BarSeries* bars = addThree(plot);
    EXPECT_EQ(bars->kind(), rocketplot::SeriesKind::BARS);
    ASSERT_EQ(bars->size(), 3U);
    EXPECT_EQ(bars->x(2), 6.5);  // the middle of its bin
    EXPECT_EQ(bars->y(1), 6.0);
    EXPECT_EQ(bars->edges().size(), 4U);
    EXPECT_EQ(bars->bin(1), (Range{.min = 3.0, .max = 5.0}));
    EXPECT_EQ(bars->binAt(3.0), 1U);
    EXPECT_EQ(bars->binAt(8.0), 2U);
    EXPECT_FALSE(bars->binAt(0.5));
    // The legend reads the bar a crosshair's x is over; a note goes on top of it.
    EXPECT_EQ(bars->nearestIndex(7.9), 2U);
    EXPECT_EQ(bars->valueAt(4.9), 6.0);
    EXPECT_FALSE(bars->valueAt(9.0));
}

TEST(BarSeries, ItsPointsChangeThroughSetBarsOnly)
{
    PlotWidget plot;
    BarSeries* bars = addThree(plot);
    EXPECT_THROW(bars->setData(std::vector{1.0}, std::vector{1.0}), std::logic_error);
    EXPECT_THROW(bars->append(1.0, 1.0), std::logic_error);
    EXPECT_THROW(bars->clear(), std::logic_error);
    // Errors are a bar's own.
    bars->setYErrors(std::vector{0.5, 0.5, 0.5});
    EXPECT_TRUE(bars->hasYErrors());
    bars->setBars(std::vector{0.0, 1.0}, std::vector{3.0});
    EXPECT_EQ(bars->size(), 1U);
    EXPECT_FALSE(bars->hasYErrors());  // they belonged to the bars that went
}

TEST(BarSeries, AutoscaleShowsThemFromZero)
{
    PlotWidget plot;
    addThree(plot);
    EXPECT_EQ(plot.yAxis()->min(), 0.0);  // exactly, with no margin below
    EXPECT_GT(plot.yAxis()->max(), 6.0);
    EXPECT_LE(plot.xAxis()->min(), 1.0);  // the edges, not the middles
    EXPECT_GE(plot.xAxis()->max(), 8.0);
    // Bars below 0 hang from it.
    PlotWidget below;
    below.addBars(std::vector{0.0, 1.0, 2.0}, std::vector{-3.0, -1.0});
    EXPECT_EQ(below.yAxis()->max(), 0.0);
    // A line through them that goes below 0 takes the range there.
    plot.addLine(std::vector{1.0, 8.0}, std::vector{-2.0, 3.0});
    EXPECT_LT(plot.yAxis()->min(), -2.0);
}

TEST(BarSeries, StyleSettings)
{
    PlotWidget plot;
    BarSeries* bars = addThree(plot);
    EXPECT_EQ(bars->fillOpacity(), 1.0);
    bars->setFillOpacity(2.0);
    EXPECT_EQ(bars->fillOpacity(), 1.0);
    bars->setFillOpacity(0.4);
    bars->setBarGap(5.0);
    EXPECT_EQ(bars->barGap(), 0.9);
    bars->setOutlineVisible(true);
    PlotWidget restored;
    addThree(restored);
    ASSERT_TRUE(restored.restoreState(plot.saveState()));
    const auto* back = qobject_cast<const BarSeries*>(restored.series().front());
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->fillOpacity(), 0.4);
    EXPECT_EQ(back->barGap(), 0.9);
    EXPECT_TRUE(back->isOutlineVisible());
    bars->resetBarGap();
    bars->resetFillOpacity();
    EXPECT_EQ(bars->barGap(), 0.0);
    EXPECT_EQ(bars->fillOpacity(), 1.0);
}

TEST(BarSeries, TheCsvHasEachBinsEdges)
{
    PlotWidget plot;
    addThree(plot, "Counts");
    EXPECT_EQ(plot.toCsv(),
              "x,Counts,Counts (bin start),Counts (bin end)\n"
              "2,2,1,3\n"
              "4,6,3,5\n"
              "6.5,4,5,8\n");
}

class BarRenderingTest : public rocketplot::test::RenderedPlotTest
{ };

TEST_F(BarRenderingTest, ARowAcrossABarIsAllBarOrAllBackground)
{
    BarSeries* bars = m_plot.addBars(std::vector{2.0, 4.3, 7.1}, std::vector{5.0, 8.0});
    m_plot.xAxis()->setRange(0.0, 10.0);
    m_plot.yAxis()->setRange(0.0, 10.0);
    m_plot.xAxis()->setGridVisible(false);
    m_plot.yAxis()->setGridVisible(false);
    const QImage image = render();
    const QColor fill  = bars->color();
    const int    row   = pixelAt(0.0, 3.0).y();
    const int    left  = pixelAt(2.0, 0.0).x();
    const int    right = pixelAt(7.1, 0.0).x();
    for (int x = left - 3; x <= right + 3; ++x)
    {
        const QColor pixel = image.pixelColor(x, row);
        EXPECT_TRUE(pixel == fill || pixel == m_plot.theme().background) << x;
        // Inside the bars, with no seam between them.
        if (x > left && x < right - 1)
        {
            EXPECT_EQ(pixel, fill) << x;
        }
    }
}

TEST_F(BarRenderingTest, TheLegendShowsAPatchOfThem)
{
    BarSeries* bars = m_plot.addBars(std::vector{1.0, 2.0}, std::vector{3.0}, "Firings");
    m_plot.legend()->setVisible(true);  // for one series too
    m_plot.legend()->setAnchor(rocketplot::LegendAnchor::OUTSIDE_RIGHT);
    const QImage image  = render();
    int          filled = 0;
    const QRect  legend = m_plot.legendArea().toAlignedRect();
    for (int y = legend.top(); y <= legend.bottom(); ++y)
    {
        for (int x = legend.left(); x <= legend.right(); ++x)
        {
            filled += image.pixelColor(x, y) == bars->color() ? 1 : 0;
        }
    }
    EXPECT_GT(filled, 100);  // a square of 14 pixels
}

TEST_F(BarRenderingTest, AnExportLooksLikeTheWidget)
{
    BarSeries* bars = m_plot.addBars(std::vector{1.0, 2.5, 4.0, 9.0}, std::vector{3.0, 7.0, 1.5});
    bars->setOutlineVisible(true);
    bars->setFillOpacity(0.5);
    bars->setYErrors(std::vector{0.5, 1.0, 0.25});
    EXPECT_EQ(m_plot.renderToImage().convertToFormat(QImage::Format_RGB32),
              render().convertToFormat(QImage::Format_RGB32));
}

TEST(BarCrosshair, ItGoesToTheBarUnderThePointer)
{
    PlotWidget plot;
    plot.resize(600, 400);
    addThree(plot);
    plot.setCrosshairEnabled(true);
    plot.setCrosshairMode(rocketplot::CrosshairMode::SNAP);
    plot.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&plot));
    QTest::mouseMove(&plot, QPoint(1, 1));
    QTest::mouseMove(&plot, plot.mapFromData(QPointF(4.5, 1.0)).toPoint());
    EXPECT_EQ(plot.crosshairSeries(), plot.series().front());
    EXPECT_EQ(plot.crosshairIndex(), 1U);
    const QPointF at = plot.crosshairPosition().value_or(QPointF(-1.0, -1.0));
    EXPECT_NEAR(at.x(), 4.0, 0.05);  // the middle of the top of the bar
    EXPECT_NEAR(at.y(), 6.0, 0.05);
}

}  // namespace
