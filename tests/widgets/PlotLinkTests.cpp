#include "rocketplot/PlotLink.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
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

// Points at the center of @p plot's plot area, and returns where that is. QTest moves the cursor
// there, which reports a move only if it was elsewhere (an earlier test may have left it there).
QPoint pointAtCenter(PlotWidget& plot)
{
    const QPoint center = plot.plotArea().center().toPoint();
    QTest::mouseMove(&plot, QPoint(1, 1));
    QTest::mouseMove(&plot, center);
    return center;
}

// One wheel notch in, at the center of @p plot.
void zoomIn(PlotWidget& plot)
{
    const QPointF position = plot.plotArea().center();
    QWheelEvent event(position, plot.mapToGlobal(position), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&plot, &event);
}

// A place on no window.
constexpr QPoint kNowhere(-1000, -1000);

// Short y labels for @p narrow and long ones for @p wide, which needs a much wider left margin:
// by themselves their plot areas don't line up.
void fillUnevenly(PlotWidget& narrow, PlotWidget& wide)
{
    narrow.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 1});
    wide.addLine(std::vector<double>{5, 20}, std::vector<double>{-123456.789, 987654.321});
    wide.yAxis()->setNumberFormat(rocketplot::NumberFormat::PLAIN);
}

class PlotLinkTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // No pointer left behind by an earlier test: a window that opens under the pointer takes
        // it for its own, and then two plots of the link each think they have it.
        QCursor::setPos(kNowhere);
        QApplication::processEvents();
        for (PlotWidget* plot : {&m_a, &m_b})
        {
            plot->resize(600, 300);
        }
        fillUnevenly(m_a, m_b);
        m_link.addPlot(&m_a);
        m_link.addPlot(&m_b);
        // On screen, where the pointer can be over them.
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

TEST_F(PlotLinkTest, HiddenPlotsTakeNoPart)
{
    const double aligned = m_a.plotArea().left();
    m_b.hide();
    EXPECT_LT(m_a.plotArea().left(), aligned);  // the margin its own labels need
    m_b.show();
    EXPECT_DOUBLE_EQ(m_a.plotArea().left(), aligned);
}

// Plots drawn before they are shown (an export, QWidget::grab()) line up as they will on screen.
TEST(PlotLink, PlotsNotShownYetLineUp)
{
    PlotWidget narrow;
    PlotWidget wide;
    fillUnevenly(narrow, wide);
    narrow.resize(600, 300);
    wide.resize(600, 300);
    PlotLink link;
    link.addPlot(&narrow);
    link.addPlot(&wide);

    EXPECT_DOUBLE_EQ(narrow.plotArea().left(), wide.plotArea().left());
    EXPECT_DOUBLE_EQ(narrow.plotArea().right(), wide.plotArea().right());
    link.setAlignMargins(false);
    EXPECT_LT(narrow.plotArea().left(), wide.plotArea().left());
}

// Two linked plots in a window that is laid out and drawn, but never shown.
class UnshownPlotLinkTest : public testing::Test
{
protected:
    void SetUp() override
    {
        auto* layout = new QVBoxLayout(&m_window);
        layout->addWidget(m_narrow);
        layout->addWidget(m_wide);
        fillUnevenly(*m_narrow, *m_wide);
        m_link.addPlot(m_narrow);
        m_link.addPlot(m_wide);
        m_window.resize(600, 600);
        ASSERT_FALSE(m_window.grab().isNull());  // lays the window out and draws it
        ASSERT_EQ(m_narrow->width(), m_wide->width());
    }

    QWidget     m_window;
    PlotWidget* m_narrow = new PlotWidget(&m_window);
    PlotWidget* m_wide   = new PlotWidget(&m_window);
    PlotLink    m_link;
};

TEST_F(UnshownPlotLinkTest, PlotAreasLineUp)
{
    EXPECT_DOUBLE_EQ(m_narrow->plotArea().left(), m_wide->plotArea().left());
    EXPECT_DOUBLE_EQ(m_narrow->plotArea().right(), m_wide->plotArea().right());
    m_link.setAlignMargins(false);
    EXPECT_LT(m_narrow->plotArea().left(), m_wide->plotArea().left());
}

TEST_F(UnshownPlotLinkTest, HiddenPlotsTakeNoPart)
{
    const double aligned = m_narrow->plotArea().left();
    m_wide->hide();
    EXPECT_LT(m_narrow->plotArea().left(), aligned);
    m_wide->show();
    EXPECT_DOUBLE_EQ(m_narrow->plotArea().left(), aligned);

    // Nor does a plot on a page that was hidden. This one's labels are longer still.
    QWidget    page(&m_window);
    PlotWidget onPage(&page);
    onPage.resize(600, 300);
    onPage.addLine(std::vector<double>{0, 10}, std::vector<double>{-1e12, 1e12});
    onPage.yAxis()->setNumberFormat(rocketplot::NumberFormat::PLAIN);
    m_link.addPlot(&onPage);
    EXPECT_GT(m_narrow->plotArea().left(), aligned);
    page.hide();
    EXPECT_DOUBLE_EQ(m_narrow->plotArea().left(), aligned);
}

// A window that was closed isn't one that is about to be shown.
TEST(PlotLink, PlotsInAClosedWindowTakeNoPart)
{
    PlotWidget narrow;
    PlotWidget wide;
    fillUnevenly(narrow, wide);
    narrow.resize(600, 300);
    wide.resize(600, 300);
    PlotLink link;
    link.addPlot(&narrow);
    link.addPlot(&wide);
    const double aligned = narrow.plotArea().left();

    wide.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&wide));
    wide.close();
    EXPECT_LT(narrow.plotArea().left(), aligned);
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
    pointAtCenter(m_a);
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

TEST_F(PlotLinkTest, ACrosshairOnTheDataShowsThePointsXInTheOtherPlots)
{
    m_a.setCrosshairEnabled(true);
    m_b.setCrosshairEnabled(true);
    m_a.setCrosshairMode(rocketplot::CrosshairMode::TRACE);

    // The line of a has points at x = 0 and x = 10 only.
    QTest::mouseMove(&m_a, QPoint(1, 1));
    QTest::mouseMove(&m_a, m_a.mapFromData(QPointF(8.0, 0.5)).toPoint());

    ASSERT_TRUE(m_b.crosshairPosition());
    EXPECT_DOUBLE_EQ(m_a.crosshairPosition().value_or(QPointF()).x(), 10.0);
    EXPECT_DOUBLE_EQ(m_b.crosshairPosition().value_or(QPointF()).x(), 10.0);
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
    const QPoint pointer = pointAtCenter(m_a);
    // A plot without the pointer moves first (new data, code): the crosshair stays everywhere,
    // at the x now under the pointer.
    m_b.xAxis()->setRange(1.0, 3.0);
    for (PlotWidget* plot : {&m_a, &m_b, &third})
    {
        plot->grab();  // as drawn: the plot areas have lined up again
    }
    ASSERT_TRUE(m_a.crosshairPosition());
    ASSERT_TRUE(third.crosshairPosition());
    EXPECT_DOUBLE_EQ(third.crosshairPosition().value_or(QPointF()).x(),
                     m_a.crosshairPosition().value_or(QPointF()).x());
    // The x under the pointer, though the plot areas moved as their labels changed.
    EXPECT_NEAR(m_a.crosshairPosition().value_or(QPointF()).x(), m_a.mapToData(pointer).x(), 1e-9);
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
