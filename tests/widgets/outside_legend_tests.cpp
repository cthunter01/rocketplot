// Legends outside the plot area: LegendAnchor::OUTSIDE_BOTTOM and OUTSIDE_RIGHT.

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTest>
#include <QWheelEvent>
#include <Qt>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::LegendAnchor;
using rocketplot::PlotWidget;

// A plot on screen with @p count lines named "Series 1", "Series 2", ...
class OutsideLegendTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        addSeries(3);
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
        QTest::mouseMove(&m_plot, QPoint(1, 1));
    }

    void addSeries(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto number = static_cast<double>(m_plot.series().size());
            m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{number, number + 1},
                           QString("Series %1").arg(m_plot.series().size() + 1));
        }
    }

    QImage repaint() { return m_plot.grab().toImage(); }

    [[nodiscard]] QRectF legend() const { return m_plot.legendArea(); }

    PlotWidget m_plot;
};

TEST_F(OutsideLegendTest, UnderOrRightOfThePlotArea)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
    repaint();
    ASSERT_FALSE(legend().isEmpty());
    EXPECT_FALSE(legend().intersects(m_plot.plotArea()));
    EXPECT_GT(legend().top(), m_plot.plotArea().bottom());
    EXPECT_LE(legend().bottom(), m_plot.height());

    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_RIGHT);
    repaint();
    ASSERT_FALSE(legend().isEmpty());
    EXPECT_FALSE(legend().intersects(m_plot.plotArea()));
    EXPECT_GT(legend().left(), m_plot.plotArea().right());
    EXPECT_LE(legend().right(), m_plot.width());
    // A column, as tall as three rows.
    EXPECT_GT(legend().height(), legend().width() / 3.0);

    // Inside again, the plot area takes the room back.
    const QRectF outside = m_plot.plotArea();
    m_plot.legend()->setAnchor(LegendAnchor::TOP_LEFT);
    repaint();
    EXPECT_GT(m_plot.plotArea().width(), outside.width() + 50.0);
    EXPECT_TRUE(m_plot.plotArea().contains(legend()));
}

TEST_F(OutsideLegendTest, ManyEntriesWrapOntoRows)
{
    addSeries(12);  // 15 in all
    m_plot.resize(600, 500);
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
    repaint();
    const double oneRow = [] {
        PlotWidget single;
        single.resize(600, 500);
        single.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1}, "Series 1");
        single.legend()->setVisible(true);
        single.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
        static_cast<void>(single.grab());
        return single.legendArea().height();
    }();
    EXPECT_LE(legend().width(), 600.0);
    EXPECT_GT(legend().height(), (2.0 * oneRow) - 16.0);  // several rows
    EXPECT_LT(legend().height(), 6.0 * oneRow);           // of several entries each
    EXPECT_FALSE(legend().intersects(m_plot.plotArea()));
}

TEST_F(OutsideLegendTest, ItTakesNoMoreThanItsShareOfTheHeight)
{
    addSeries(57);  // 60 in all
    m_plot.resize(400, 300);
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
    repaint();
    EXPECT_LE(legend().height(), 0.4 * 300.0);
    EXPECT_FALSE(m_plot.plotArea().isEmpty());
}

TEST_F(OutsideLegendTest, ItsEntriesAreClickedButItIsNotDragged)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_RIGHT);
    repaint();
    const QPoint first = (legend().topLeft() + QPointF(40.0, 14.0)).toPoint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, first);
    EXPECT_FALSE(m_plot.series().front()->isVisible());
    repaint();
    const QRectF before = legend();
    QTest::mousePress(&m_plot, Qt::LeftButton, Qt::NoModifier, first);
    QTest::mouseMove(&m_plot, first - QPoint(150, 0));
    QTest::mouseMove(&m_plot, first - QPoint(300, 0));
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, first - QPoint(300, 0));
    repaint();
    EXPECT_EQ(m_plot.legend()->anchor(), LegendAnchor::OUTSIDE_RIGHT);
    EXPECT_EQ(legend(), before);
    EXPECT_TRUE(m_plot.xAxis()->autoscale());  // nor did the plot pan
}

TEST_F(OutsideLegendTest, ThePlotAreaHoldsWhileTheCrosshairMoves)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_RIGHT);
    m_plot.setCrosshairEnabled(true);
    repaint();
    const QRectF area = m_plot.plotArea();
    const QRectF box  = legend();
    for (const double x : {0.5, 3.3, 9.9})
    {
        QTest::mouseMove(&m_plot, m_plot.mapFromData(QPointF(x, 1.0)).toPoint());
        repaint();
        EXPECT_EQ(m_plot.plotArea(), area) << x;
        EXPECT_EQ(legend(), box) << x;
    }
}

TEST_F(OutsideLegendTest, AWheelOverItDoesNothing)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
    repaint();
    const rocketplot::Range x    = m_plot.xAxis()->range();
    const QPointF           over = legend().center();
    QWheelEvent event(over, m_plot.mapToGlobal(over), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&m_plot, &event);
    EXPECT_EQ(m_plot.xAxis()->range(), x);
}

TEST_F(OutsideLegendTest, ItHasNoFrame)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_BOTTOM);
    const QImage image = repaint();
    const QPoint corner((legend().topLeft() + QPointF(4.0, 0.5)).toPoint());
    EXPECT_EQ(image.pixelColor(corner), m_plot.theme().background);
}

TEST_F(OutsideLegendTest, ExportsLookLikeTheWidget)
{
    for (const LegendAnchor anchor : {LegendAnchor::OUTSIDE_BOTTOM, LegendAnchor::OUTSIDE_RIGHT})
    {
        m_plot.legend()->setAnchor(anchor);
        EXPECT_EQ(m_plot.renderToImage().convertToFormat(QImage::Format_RGB32),
                  repaint().convertToFormat(QImage::Format_RGB32))
            << static_cast<int>(anchor);
    }
}

TEST_F(OutsideLegendTest, ItIsPartOfTheState)
{
    m_plot.legend()->setAnchor(LegendAnchor::OUTSIDE_RIGHT);
    PlotWidget restored;
    restored.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1}, "Series 1");
    ASSERT_TRUE(restored.restoreState(m_plot.saveState()));
    EXPECT_EQ(restored.legend()->anchor(), LegendAnchor::OUTSIDE_RIGHT);
}

TEST(OutsideLegend, PlotAreasOfAGridStillLineUp)
{
    rocketplot::PlotGrid grid(2, 1);
    grid.resize(800, 600);
    for (int row = 0; row < 2; ++row)
    {
        PlotWidget* plot = grid.plot(row, 0);
        plot->addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1}, "Up");
        plot->addLine(std::vector<double>{0, 1}, std::vector<double>{1, 0}, "Down");
    }
    grid.plot(0, 0)->legend()->setAnchor(LegendAnchor::OUTSIDE_RIGHT);
    static_cast<void>(grid.grab());
    EXPECT_DOUBLE_EQ(grid.plot(0, 0)->plotArea().right(), grid.plot(1, 0)->plotArea().right());
    EXPECT_LT(grid.plot(1, 0)->plotArea().right(), 760.0);  // made room as its neighbor did
}

}  // namespace
