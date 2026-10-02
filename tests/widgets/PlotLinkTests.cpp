#include "rocketplot/PlotLink.h"

#include <QApplication>
#include <QEvent>
#include <QPoint>
#include <QPointF>
#include <QTest>
#include <QWheelEvent>
#include <Qt>
#include <cmath>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotLink;
using rocketplot::PlotWidget;
using rocketplot::Range;

// One wheel notch in, at the center of @p plot.
void zoomIn(PlotWidget& plot)
{
    const QPointF position = plot.plotArea().center();
    QWheelEvent event(position, plot.mapToGlobal(position), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&plot, &event);
}

class PlotLinkTest : public testing::Test
{
protected:
    void SetUp() override
    {
        for (PlotWidget* plot : {&m_a, &m_b})
        {
            plot->resize(600, 300);
        }
        m_a.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 1});
        // Long y labels: plot b needs a much wider left margin.
        m_b.addLine(std::vector<double>{5, 20}, std::vector<double>{-123456.789, 987654.321});
        m_b.yAxis()->setNumberFormat(rocketplot::NumberFormat::PLAIN);
        m_link.addPlot(&m_a);
        m_link.addPlot(&m_b);
        // Margins are aligned between plots that are shown.
        m_a.show();
        m_b.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_a));
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_b));
    }

    PlotWidget m_a;
    PlotWidget m_b;
    PlotLink   m_link;
};

TEST_F(PlotLinkTest, AutoscaleFitsEveryPlotsData)
{
    EXPECT_EQ(m_a.xAxis()->range(), m_b.xAxis()->range());
    EXPECT_LT(m_a.xAxis()->min(), 0.0);
    EXPECT_GT(m_a.xAxis()->max(), 20.0);
}

TEST_F(PlotLinkTest, RangeChangesFollow)
{
    m_a.xAxis()->setRange(2.0, 4.0);
    EXPECT_EQ(m_b.xAxis()->range(), (Range{.min = 2.0, .max = 4.0}));
    EXPECT_FALSE(m_b.xAxis()->autoscale());
    m_b.resetView();
    EXPECT_TRUE(m_a.xAxis()->autoscale());
    EXPECT_EQ(m_a.xAxis()->range(), m_b.xAxis()->range());
    EXPECT_GT(m_a.xAxis()->max(), 20.0);
}

TEST_F(PlotLinkTest, YAxesStayIndependent)
{
    m_a.yAxis()->setRange(0.25, 0.5);
    EXPECT_NE(m_b.yAxis()->range(), m_a.yAxis()->range());
}

TEST_F(PlotLinkTest, PlotAreasLineUp)
{
    EXPECT_DOUBLE_EQ(m_a.plotArea().left(), m_b.plotArea().left());
    EXPECT_DOUBLE_EQ(m_a.plotArea().right(), m_b.plotArea().right());
    m_link.setAlignMargins(false);
    EXPECT_LT(m_a.plotArea().left(), m_b.plotArea().left());
}

TEST_F(PlotLinkTest, RemovedPlotsGoTheirOwnWay)
{
    m_link.removePlot(&m_b);
    EXPECT_EQ(m_b.link(), nullptr);
    m_a.xAxis()->setRange(2.0, 4.0);
    EXPECT_NE(m_b.xAxis()->range(), m_a.xAxis()->range());
    EXPECT_EQ(m_link.plots().size(), 1);
}

TEST_F(PlotLinkTest, DeletedPlotsLeave)
{
    auto plot = std::make_unique<PlotWidget>();
    m_link.addPlot(plot.get());
    EXPECT_EQ(m_link.plots().size(), 3);
    plot.reset();
    EXPECT_EQ(m_link.plots().size(), 2);
    m_a.xAxis()->setRange(1.0, 3.0);  // no dangling plot to sync
    EXPECT_EQ(m_b.xAxis()->range(), (Range{.min = 1.0, .max = 3.0}));
}

TEST_F(PlotLinkTest, APlotJoinsOneLinkAtATime)
{
    PlotLink other;
    other.addPlot(&m_b);
    EXPECT_EQ(m_b.link(), &other);
    EXPECT_EQ(m_link.plots().size(), 1);
}

TEST_F(PlotLinkTest, CrosshairShowsInTheOtherPlots)
{
    m_a.setCrosshairEnabled(true);
    m_b.setCrosshairEnabled(true);
    QTest::mouseMove(&m_a, m_a.plotArea().center().toPoint());
    ASSERT_TRUE(m_a.crosshairPosition());
    ASSERT_TRUE(m_b.crosshairPosition());
    const QPointF a = m_a.crosshairPosition().value_or(QPointF());
    const QPointF b = m_b.crosshairPosition().value_or(QPointF());
    EXPECT_DOUBLE_EQ(b.x(), a.x());
    EXPECT_TRUE(std::isnan(b.y()));

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&m_a, &leave);
    EXPECT_FALSE(m_b.crosshairPosition());

    m_link.setLinkCrosshair(false);
    QTest::mouseMove(&m_a, m_a.plotArea().center().toPoint() + QPoint(10, 0));
    EXPECT_TRUE(m_a.crosshairPosition());
    EXPECT_FALSE(m_b.crosshairPosition());
}

TEST_F(PlotLinkTest, CrosshairStaysWhenAnotherPlotMoves)
{
    PlotWidget third;
    third.resize(600, 300);
    third.addLine(std::vector<double>{0, 1});
    m_link.addPlot(&third);
    third.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&third));
    for (PlotWidget* plot : {&m_a, &m_b, &third})
    {
        plot->setCrosshairEnabled(true);
    }
    QTest::mouseMove(&m_a, m_a.plotArea().center().toPoint());
    // A plot without the pointer moves first (new data, code): the crosshair stays everywhere,
    // at the x now under the pointer.
    m_b.xAxis()->setRange(1.0, 3.0);
    ASSERT_TRUE(m_a.crosshairPosition());
    ASSERT_TRUE(third.crosshairPosition());
    EXPECT_DOUBLE_EQ(third.crosshairPosition().value_or(QPointF()).x(),
                     m_a.crosshairPosition().value_or(QPointF()).x());
    EXPECT_NEAR(m_a.crosshairPosition().value_or(QPointF()).x(), 2.0, 0.01);
}

TEST_F(PlotLinkTest, PlotsShareTheirHistory)
{
    const Range x = m_a.xAxis()->range();
    const Range y = m_a.yAxis()->range();
    zoomIn(m_a);
    EXPECT_TRUE(m_b.canGoBack());
    m_b.back();  // undoes the zoom in a
    EXPECT_EQ(m_a.xAxis()->range(), x);
    EXPECT_EQ(m_a.yAxis()->range(), y);
    EXPECT_TRUE(m_a.yAxis()->autoscale());
    EXPECT_TRUE(m_a.canGoForward());

    // Joining or leaving starts the history afresh.
    zoomIn(m_b);
    m_link.removePlot(&m_b);
    EXPECT_FALSE(m_a.canGoBack());
    EXPECT_FALSE(m_b.canGoBack());
}

}  // namespace
