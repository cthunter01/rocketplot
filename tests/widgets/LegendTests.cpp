#include "rocketplot/Legend.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QContextMenuEvent>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTest>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::LegendAnchor;
using rocketplot::LineSeries;
using rocketplot::PlotWidget;
using rocketplot::Series;

QPoint pixel(QPointF point)
{
    return point.toPoint();
}

// The action called @p text in @p menu or its submenus.
QAction* findAction(const QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions())
    {
        if (action->text() == text)
        {
            return action;
        }
        if (const QMenu* submenu = action->menu())
        {
            if (QAction* found = findAction(*submenu, text))
            {
                return found;
            }
        }
    }
    return nullptr;
}

TEST(Legend, ShownForTwoEntriesByDefault)
{
    PlotWidget          plot;
    rocketplot::Legend* legend = plot.legend();
    EXPECT_FALSE(legend->isShownFor(0));
    EXPECT_FALSE(legend->isShownFor(1));
    EXPECT_TRUE(legend->isShownFor(2));
    legend->setVisible(true);
    EXPECT_TRUE(legend->isShownFor(1));
    legend->setVisible(false);
    EXPECT_FALSE(legend->isShownFor(3));
    legend->resetVisible();
    EXPECT_TRUE(legend->isShownFor(2));
}

TEST(Legend, PositionMakesItCustomAndStaysInside)
{
    PlotWidget          plot;
    rocketplot::Legend* legend = plot.legend();
    EXPECT_EQ(legend->anchor(), LegendAnchor::BEST);
    legend->setPosition(QPointF(2.0, -1.0));
    EXPECT_EQ(legend->anchor(), LegendAnchor::CUSTOM);
    EXPECT_EQ(legend->position(), QPointF(1.0, 0.0));
}

// Two curves from the bottom left to the top right, and a flat line along the bottom: the top
// left of the plot is empty.
class LegendTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        std::vector<double> x;
        std::vector<double> square;
        std::vector<double> cube;
        for (int i = 0; i <= 100; ++i)
        {
            const double t = i / 10.0;
            x.push_back(t);
            square.push_back(t * t);
            cube.push_back(t * t * t / 20.0);
        }
        m_square = m_plot.addLine(x, square, "square");
        m_cube   = m_plot.addLine(x, cube, "cube");
        m_flat   = m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{1, 1}, "flat");
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
        repaint();
    }

    void TearDown() override
    {
        while (QWidget* popup = QApplication::activePopupWidget())
        {
            popup->close();
        }
    }

    // Draws the plot, so the legend's place is known.
    QImage repaint() { return m_plot.grab().toImage(); }

    [[nodiscard]] QRectF legend() const { return m_plot.legendArea(); }

    // Right-clicks @p position and returns the menu that opened.
    QMenu* openEntryMenu(QPoint position)
    {
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, m_plot.mapToGlobal(position));
        QApplication::sendEvent(&m_plot, &event);
        return qobject_cast<QMenu*>(QApplication::activePopupWidget());
    }
    // A point in the first and the last entry's row.
    [[nodiscard]] QPoint firstEntry() const { return pixel(legend().topLeft() + QPointF(40, 14)); }
    [[nodiscard]] QPoint lastEntry() const
    {
        return pixel(legend().bottomLeft() + QPointF(40, -14));
    }

    PlotWidget  m_plot;
    LineSeries* m_square = nullptr;
    LineSeries* m_cube   = nullptr;
    LineSeries* m_flat   = nullptr;
};

TEST_F(LegendTest, BestPlaceAvoidsTheData)
{
    const QRectF area = m_plot.plotArea();
    ASSERT_FALSE(legend().isEmpty());
    EXPECT_LT(legend().center().x(), area.center().x());  // top left, not top right
    EXPECT_LT(legend().center().y(), area.center().y());

    m_plot.legend()->setAnchor(LegendAnchor::BOTTOM_RIGHT);
    repaint();
    EXPECT_GT(legend().center().x(), area.center().x());
    EXPECT_GT(legend().center().y(), area.center().y());
}

TEST_F(LegendTest, ClickTogglesASeries)
{
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, firstEntry());
    EXPECT_FALSE(m_square->isVisible());
    EXPECT_LT(m_plot.yAxis()->max(), 90.0);    // autoscale fits what's shown
    EXPECT_TRUE(m_plot.xAxis()->autoscale());  // no pan
    repaint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, firstEntry());
    EXPECT_TRUE(m_square->isVisible());
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, lastEntry());
    EXPECT_FALSE(m_flat->isVisible());
}

TEST_F(LegendTest, DoubleClickShowsOnlyThatSeries)
{
    m_plot.xAxis()->setRange(1.0, 9.0);
    QTest::mouseDClick(&m_plot, Qt::LeftButton, Qt::NoModifier, lastEntry());
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, lastEntry());
    EXPECT_TRUE(m_flat->isVisible());
    EXPECT_FALSE(m_square->isVisible());
    EXPECT_FALSE(m_cube->isVisible());
    EXPECT_FALSE(m_plot.xAxis()->autoscale());  // not a reset
    repaint();
    QTest::mouseDClick(&m_plot, Qt::LeftButton, Qt::NoModifier, lastEntry());
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, lastEntry());
    EXPECT_TRUE(m_square->isVisible());
    EXPECT_TRUE(m_cube->isVisible());
}

TEST_F(LegendTest, DraggingMovesIt)
{
    const QRectF before = legend();
    const QPoint start  = pixel(before.topLeft() + QPointF(before.width() / 2.0, 4.0));
    QTest::mousePress(&m_plot, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&m_plot, start + QPoint(150, 50));
    QTest::mouseMove(&m_plot, start + QPoint(300, 100));
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, start + QPoint(300, 100));
    repaint();
    EXPECT_EQ(m_plot.legend()->anchor(), LegendAnchor::CUSTOM);
    EXPECT_NEAR(legend().left(), before.left() + 300.0, 1.0);
    EXPECT_NEAR(legend().top(), before.top() + 100.0, 1.0);
    EXPECT_TRUE(m_plot.xAxis()->autoscale());  // the plot didn't pan
    EXPECT_TRUE(m_square->isVisible());        // nor was an entry clicked
}

TEST_F(LegendTest, PointingAtAnEntryHighlightsItsSeries)
{
    // A point on the flat line, away from the others.
    const QPoint onFlat = pixel(m_plot.mapFromData(QPointF(9.0, 1.0)));
    const QImage before = repaint();
    QTest::mouseMove(&m_plot, firstEntry());  // the square curve's entry
    const QImage pointed = repaint();
    EXPECT_NE(pointed.pixelColor(onFlat), before.pixelColor(onFlat));  // faded
    EXPECT_EQ(m_plot.cursor().shape(), Qt::PointingHandCursor);
    QTest::mouseMove(&m_plot, pixel(m_plot.plotArea().center()));
    EXPECT_EQ(repaint().pixelColor(onFlat), before.pixelColor(onFlat));
}

TEST_F(LegendTest, ShowsValuesAtTheCrosshair)
{
    const double plain = legend().width();
    m_plot.setCrosshairEnabled(true);
    QTest::mouseMove(&m_plot, pixel(m_plot.mapFromData(QPointF(8.0, 20.0))));
    repaint();
    EXPECT_GT(legend().width(), plain + 10.0);  // a column of values
    m_plot.legend()->setValuesVisible(false);
    repaint();
    EXPECT_DOUBLE_EQ(legend().width(), plain);
}

TEST_F(LegendTest, EntryMenuIsForThatSeries)
{
    const Series* menuSeries = nullptr;
    QObject::connect(m_plot.legend(), &rocketplot::Legend::entryMenuAboutToShow,
                     [&](QMenu* /*menu*/, Series* series) { menuSeries = series; });
    const QMenu* menu = openEntryMenu(firstEntry());
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menuSeries, m_square);
    EXPECT_NE(findAction(*menu, "Color…"), nullptr);
    EXPECT_EQ(findAction(*menu, "Theme color"), nullptr);  // its color is the theme's
}

TEST_F(LegendTest, EntryMenuChangesTheStyle)
{
    const QMenu* menu = openEntryMenu(firstEntry());
    ASSERT_NE(menu, nullptr);
    QAction* width = findAction(*menu, "3 px");
    ASSERT_NE(width, nullptr);
    width->trigger();
    EXPECT_DOUBLE_EQ(m_square->lineWidth(), 3.0);
    QAction* square = findAction(*menu, "Square");
    ASSERT_NE(square, nullptr);
    square->trigger();
    EXPECT_EQ(m_square->marker(), rocketplot::Marker::SQUARE);
}

TEST_F(LegendTest, EntryMenuShowsAndRemoves)
{
    const QMenu* menu = openEntryMenu(firstEntry());
    ASSERT_NE(menu, nullptr);
    QAction* only = findAction(*menu, "Show only this");
    ASSERT_NE(only, nullptr);
    only->trigger();
    EXPECT_TRUE(m_square->isVisible());
    EXPECT_FALSE(m_flat->isVisible());
    QAction* remove = findAction(*menu, "Remove");
    ASSERT_NE(remove, nullptr);
    remove->trigger();
    EXPECT_EQ(m_plot.series().size(), 2);
}

TEST_F(LegendTest, NotInteractive)
{
    m_plot.legend()->setInteractive(false);
    repaint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, firstEntry());
    EXPECT_TRUE(m_square->isVisible());
    // Its context menu is the plot's.
    QContextMenuEvent event(QContextMenuEvent::Mouse, firstEntry(),
                            m_plot.mapToGlobal(firstEntry()));
    QApplication::sendEvent(&m_plot, &event);
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    ASSERT_NE(menu, nullptr);
    EXPECT_NE(findAction(*menu, "Reset view"), nullptr);
    menu->close();
}

}  // namespace
