// Legend entries that stand for several series (Series::legendGroup()) or for an annotation
// (Annotation::setLegendName()).

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QContextMenuEvent>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/RectAnnotation.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/Series.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::Annotation;
using rocketplot::LineSeries;
using rocketplot::PlotWidget;
using rocketplot::Series;

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

// Whether some pixel of @p area of @p image is @p color.
bool hasColor(const QImage& image, const QRect& area, const QColor& color)
{
    for (int y = area.top(); y <= area.bottom(); ++y)
    {
        for (int x = area.left(); x <= area.right(); ++x)
        {
            if (image.pixelColor(x, y) == color)
            {
                return true;
            }
        }
    }
    return false;
}

// Whether something is drawn in @p area of @p image: a pixel unlike the one left of it.
bool hasInk(const QImage& image, const QRect& area)
{
    const QColor background = image.pixelColor(area.topLeft() - QPoint(4, 0));
    for (int y = area.top(); y <= area.bottom(); ++y)
    {
        for (int x = area.left(); x <= area.right(); ++x)
        {
            if (image.pixelColor(x, y) != background)
            {
                return true;
            }
        }
    }
    return false;
}

// Whether every series of @p series is visible, or none.
bool allVisible(const std::vector<LineSeries*>& series)
{
    return std::ranges::all_of(series, [](const Series* one) { return one->isVisible(); });
}
bool noneVisible(const std::vector<LineSeries*>& series)
{
    return std::ranges::none_of(series, [](const Series* one) { return one->isVisible(); });
}

// A plot on screen, its legend at the top left; one flight in three stages, each stage's altitude
// a series of the legend group "altitude" (only the first named), and a speed of its own.
class LegendEntriesTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        m_plot.legend()->setAnchor(rocketplot::LegendAnchor::TOP_LEFT);
        for (int stage = 0; stage < 3; ++stage)
        {
            const double start = stage * 3.0;
            LineSeries*  line  = m_plot.addLine(std::vector<double>{start, start + 3.0},
                                                std::vector<double>{start, start + 3.0},
                                                stage == 0 ? QString("Altitude") : QString());
            line->setLegendGroup("altitude");
            m_stages.push_back(line);
        }
        m_speed = m_plot.addLine(std::vector<double>{0, 9}, std::vector<double>{8, 8}, "Speed");
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
        QTest::mouseMove(&m_plot, QPoint(1, 1));
        repaint();
    }

    void TearDown() override
    {
        while (QWidget* popup = QApplication::activePopupWidget())
        {
            popup->close();
        }
    }

    QImage repaint() { return m_plot.grab().toImage(); }

    [[nodiscard]] QRectF legend() const { return m_plot.legendArea(); }
    // A point in the row of entry @p row of @p rows.
    [[nodiscard]] QPoint entry(int row, int rows) const
    {
        const double first = legend().top() + 14.0;
        const double last  = legend().bottom() - 14.0;
        const double y     = rows > 1 ? first + ((last - first) * row / (rows - 1)) : first;
        return QPointF(legend().left() + 40.0, y).toPoint();
    }
    // Where entry @p row of @p rows has its swatch.
    [[nodiscard]] QRect swatch(int row, int rows) const
    {
        const QPoint middle(static_cast<int>(legend().left()) + 20, entry(row, rows).y());
        return {middle - QPoint(12, 7), middle + QPoint(12, 7)};
    }

    QMenu* openEntryMenu(QPoint position)
    {
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, m_plot.mapToGlobal(position));
        QApplication::sendEvent(&m_plot, &event);
        return qobject_cast<QMenu*>(QApplication::activePopupWidget());
    }

    PlotWidget               m_plot;
    std::vector<LineSeries*> m_stages;
    LineSeries*              m_speed = nullptr;
};

TEST_F(LegendEntriesTest, AGroupHasOneEntryNamedAfterItsFirstNamedSeries)
{
    // As tall and as wide as a legend of two series named so.
    PlotWidget plain;
    plain.resize(800, 600);
    plain.addLine(std::vector<double>{0, 9}, std::vector<double>{0, 9}, "Altitude");
    plain.addLine(std::vector<double>{0, 9}, std::vector<double>{8, 8}, "Speed");
    static_cast<void>(plain.grab());
    EXPECT_EQ(legend().size(), plain.legendArea().size());

    // Unnamed series before the first named one join it; the entry looks like the named one.
    m_stages[0]->setName({});
    m_stages[1]->setName("Altitude");
    m_stages[1]->setColor(Qt::red);
    const QImage image = repaint();
    EXPECT_EQ(legend().size(), plain.legendArea().size());
    EXPECT_TRUE(hasColor(image, swatch(0, 2), Qt::red));
    // A group without a named series has no entry.
    m_stages[1]->setName({});
    repaint();
    EXPECT_TRUE(legend().isEmpty());  // one entry left: none shown
}

TEST_F(LegendEntriesTest, AClickHidesOrShowsTheWholeGroup)
{
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(0, 2));
    EXPECT_TRUE(noneVisible(m_stages));
    EXPECT_TRUE(m_speed->isVisible());
    repaint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(0, 2));
    EXPECT_TRUE(allVisible(m_stages));
    // With some of them hidden, a click hides the rest.
    m_stages[1]->setVisible(false);
    repaint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(0, 2));
    EXPECT_FALSE(m_stages[0]->isVisible());
    EXPECT_FALSE(m_stages[2]->isVisible());
}

TEST_F(LegendEntriesTest, ADoubleClickShowsOnlyTheGroup)
{
    QTest::mouseDClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(0, 2));
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(0, 2));
    EXPECT_TRUE(allVisible(m_stages));
    EXPECT_FALSE(m_speed->isVisible());
}

TEST_F(LegendEntriesTest, PointingAtItHighlightsTheWholeGroup)
{
    // A point on each stage, and on the speed, away from the legend.
    const auto at = [this](double x, double y) {
        return m_plot.mapFromData(QPointF(x, y)).toPoint();
    };
    const QImage before = repaint();
    QTest::mouseMove(&m_plot, entry(0, 2));
    const QImage pointed = repaint();
    for (const QPoint stage : {at(4.5, 4.5), at(7.5, 7.5)})
    {
        EXPECT_EQ(pointed.pixelColor(stage), before.pixelColor(stage));
    }
    EXPECT_NE(pointed.pixelColor(at(2.0, 8.0)), before.pixelColor(at(2.0, 8.0)));  // faded
}

TEST_F(LegendEntriesTest, ItsMenuIsForTheSeriesItLooksLike)
{
    Series* menuSeries = nullptr;
    QObject::connect(m_plot.legend(), &rocketplot::Legend::entryMenuAboutToShow,
                     [&](QMenu* /*menu*/, Series* series) { menuSeries = series; });
    const QMenu* menu = openEntryMenu(entry(0, 2));
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menuSeries, m_stages[0]);
    QAction* remove = findAction(*menu, "Remove");
    ASSERT_NE(remove, nullptr);
    remove->trigger();
    EXPECT_EQ(m_plot.series().size(), 1);  // the whole group
}

TEST_F(LegendEntriesTest, ItsMenuStylesTheWholeGroup)
{
    const QMenu* menu = openEntryMenu(entry(0, 2));
    ASSERT_NE(menu, nullptr);
    QAction* square = findAction(*menu, "Square");
    QAction* width  = findAction(*menu, "3 px");
    ASSERT_TRUE(square != nullptr && width != nullptr);
    square->trigger();
    width->trigger();
    EXPECT_TRUE(std::ranges::all_of(m_stages, [](const LineSeries* stage) {
        return stage->marker() == rocketplot::Marker::SQUARE && stage->lineWidth() == 3.0;
    }));
    EXPECT_EQ(m_speed->marker(), rocketplot::Marker::NONE);
}

TEST_F(LegendEntriesTest, ItsValueIsOfTheSeriesThatHasOneAtTheCrosshair)
{
    // The third stage's values are long numbers: the value column is wider where it has one.
    m_stages[2]->setData(std::vector<double>{6, 9}, std::vector<double>{123456.789, 123456.789});
    m_plot.yAxis()->setRange(0.0, 10.0);
    m_plot.setCrosshairEnabled(true);
    const auto widthAt = [this](double x) {
        QTest::mouseMove(&m_plot, m_plot.mapFromData(QPointF(x, 5.0)).toPoint());
        repaint();
        return legend().width();
    };
    const double first = widthAt(1.0);
    m_plot.legend()->setValuesVisible(true);
    EXPECT_GT(widthAt(8.0), first + 20.0);
}

// Annotations in the legend.

QIcon redIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::red);
    return {pixmap};
}

TEST_F(LegendEntriesTest, AnAnnotationWithALegendNameHasAnEntryAfterTheSeries)
{
    rocketplot::ReferenceLine* nominal = m_plot.addHorizontalLine(5.0);
    nominal->setColor(Qt::red);
    nominal->setLineStyle(Qt::SolidLine);
    nominal->setLineWidth(3.0);
    const double twoRows = legend().height();
    nominal->setLegendName("Nominal");
    const QImage image = repaint();
    EXPECT_GT(legend().height(), twoRows + 10.0);
    EXPECT_TRUE(hasColor(image, swatch(2, 3), Qt::red));

    // A click hides and shows it.
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(2, 3));
    EXPECT_FALSE(nominal->isVisible());
    repaint();
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, entry(2, 3));
    EXPECT_TRUE(nominal->isVisible());
}

TEST_F(LegendEntriesTest, EveryKindOfAnnotationHasASample)
{
    const QList<Annotation*> annotations{
        m_plot.addHorizontalLine(5.0),
        m_plot.addVerticalSpan(1.0, 2.0),
        m_plot.addRect(QRectF(1.0, 1.0, 2.0, 2.0)),
        m_plot.addEvent(3.0, "Event"),
        m_plot.addIcon(4.0, 4.0, redIcon()),
        m_plot.addText(5.0, 5.0, "Text"),
    };
    for (Annotation* annotation : annotations)
    {
        annotation->setLegendName(annotation->metaObject()->className());
        annotation->setColor(Qt::red);
    }
    const QImage image = repaint();
    const int    rows  = 2 + static_cast<int>(annotations.size());
    for (int row = 2; row < rows; ++row)
    {
        EXPECT_TRUE(hasInk(image, swatch(row, rows)))
            << annotations.at(row - 2)->metaObject()->className();
    }
}

TEST_F(LegendEntriesTest, AnAnnotationEntryHasAMenuOfItsOwn)
{
    rocketplot::ReferenceLine* mean = m_plot.addHorizontalLine(5.0);
    mean->setLegendName("Mean");
    repaint();
    Annotation* menuAnnotation = nullptr;
    QObject::connect(m_plot.legend(), &rocketplot::Legend::annotationEntryMenuAboutToShow,
                     [&](QMenu* /*menu*/, Annotation* annotation) { menuAnnotation = annotation; });
    const QMenu* menu = openEntryMenu(entry(2, 3));
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menuAnnotation, mean);
    EXPECT_NE(findAction(*menu, "Color…"), nullptr);
    EXPECT_EQ(findAction(*menu, "Marker"), nullptr);
    QAction* visible = findAction(*menu, "Visible");
    ASSERT_NE(visible, nullptr);
    visible->trigger();
    EXPECT_FALSE(mean->isVisible());
}

TEST_F(LegendEntriesTest, AnAnnotationEntrysMenuRemovesIt)
{
    m_plot.addHorizontalLine(5.0)->setLegendName("Mean");
    repaint();
    const QMenu* menu = openEntryMenu(entry(2, 3));
    ASSERT_NE(menu, nullptr);
    QAction* remove = findAction(*menu, "Remove");
    ASSERT_NE(remove, nullptr);
    remove->trigger();
    EXPECT_TRUE(m_plot.annotations().isEmpty());
}

}  // namespace
