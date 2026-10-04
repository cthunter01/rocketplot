#include "PropertyInspector.h"

#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QPointF>
#include <QString>
#include <QTest>
#include <QTimeZone>
#include <QTreeWidgetItem>
#include <Qt>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::LineSeries;
using rocketplot::PlotWidget;
using rocketplot::demo::PropertyInspector;

constexpr int kValue = 1;  // the column of the values

class PropertyInspectorTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        m_plot.setTitle("Ascent");
        m_line  = m_plot.addLine(std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 1.0}, "A");
        m_limit = m_plot.addHorizontalLine(0.5, "Limit");
        m_inspector.resize(400, 600);
        m_inspector.setPlots({&m_plot});
        m_inspector.show();  // an inspector nobody sees doesn't keep itself up to date
    }

    // The row of a property; fails the test if there is none.
    [[nodiscard]] QTreeWidgetItem* row(const QObject* object, const char* property) const
    {
        QTreeWidgetItem* item = m_inspector.propertyItem(object, property);
        EXPECT_NE(item, nullptr) << property;
        return item;
    }

    [[nodiscard]] QString shown(const QObject* object, const char* property) const
    {
        const QTreeWidgetItem* item = m_inspector.propertyItem(object, property);
        return item != nullptr ? item->text(kValue) : QStringLiteral("(no such row)");
    }

    // Lets what was scheduled happen: the inspector catches up with the plot between events.
    static void settle() { QCoreApplication::processEvents(); }

    PlotWidget                 m_plot;
    LineSeries*                m_line  = nullptr;
    rocketplot::ReferenceLine* m_limit = nullptr;
    PropertyInspector          m_inspector;
};

TEST_F(PropertyInspectorTest, ListsThePropertiesOfAPlotAndItsParts)
{
    EXPECT_NE(m_inspector.propertyItem(&m_plot, "title"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_plot.xAxis(), "min"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_plot.yAxis2(), "scaleType"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_plot.legend(), "anchor"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_line, "lineWidth"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_line, "name"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(m_limit, "value"), nullptr);
    // Its own properties only: not what every widget and object has.
    EXPECT_EQ(m_inspector.propertyItem(&m_plot, "geometry"), nullptr);
    EXPECT_EQ(m_inspector.propertyItem(m_line, "objectName"), nullptr);
}

TEST_F(PropertyInspectorTest, NamesTheParts)
{
    EXPECT_EQ(m_inspector.topLevelItemCount(), 1);
    EXPECT_EQ(m_inspector.objectItem(&m_plot)->text(0), QString("Plot: Ascent"));
    EXPECT_EQ(m_inspector.objectItem(m_plot.xAxis())->text(0), QString("X axis"));
    EXPECT_EQ(m_inspector.objectItem(m_plot.yAxis2())->text(0), QString("Y axis 2 (right)"));
    EXPECT_EQ(m_inspector.objectItem(m_line)->text(0), QString("A (LineSeries)"));
    EXPECT_EQ(m_inspector.objectItem(m_limit)->text(0), QString("ReferenceLine: Limit"));
}

TEST_F(PropertyInspectorTest, ShowsEachKindOfValue)
{
    m_plot.xAxis()->setRange(0.25, 4.0);
    m_line->setColor(QColor(255, 136, 0));
    settle();

    EXPECT_EQ(shown(&m_plot, "title"), QString("Ascent"));
    EXPECT_EQ(shown(m_plot.xAxis(), "min"), QString("0.25"));
    EXPECT_EQ(shown(m_plot.xAxis(), "scaleType"), QString("LINEAR"));
    EXPECT_EQ(shown(m_plot.xAxis(), "timeZone"), QString("UTC"));
    EXPECT_EQ(shown(m_line, "color"), QString("#ff8800"));
    EXPECT_EQ(shown(m_line, "lineStyle"), QString("SolidLine"));
    EXPECT_EQ(shown(m_plot.legend(), "position"), QString("1, 0"));
    EXPECT_EQ(shown(m_limit, "labelAlignment"), QString("Right | Top"));
    EXPECT_EQ(shown(m_limit, "orientation"), QString("Horizontal"));
    EXPECT_EQ(row(m_plot.xAxis(), "gridVisible")->checkState(kValue), Qt::Checked);
    EXPECT_EQ(row(m_plot.xAxis(), "minorGridVisible")->checkState(kValue), Qt::Unchecked);
}

TEST_F(PropertyInspectorTest, FollowsThePlotAsItChanges)
{
    m_plot.xAxis()->setRange(2.0, 3.0);
    m_plot.setCrosshairEnabled(true);
    m_line->setName("Renamed");
    settle();

    EXPECT_EQ(shown(m_plot.xAxis(), "min"), QString("2"));
    EXPECT_EQ(shown(m_plot.xAxis(), "max"), QString("3"));
    EXPECT_EQ(row(m_plot.xAxis(), "autoscale")->checkState(kValue), Qt::Unchecked);
    EXPECT_EQ(row(&m_plot, "crosshairEnabled")->checkState(kValue), Qt::Checked);
    EXPECT_EQ(m_inspector.objectItem(m_line)->text(0), QString("Renamed (LineSeries)"));
}

TEST_F(PropertyInspectorTest, TextSetsANumber)
{
    EXPECT_TRUE(m_inspector.setValue(row(m_plot.xAxis(), "max"), QString(" 2.5 ")));

    EXPECT_DOUBLE_EQ(m_plot.xAxis()->max(), 2.5);
    EXPECT_EQ(shown(m_plot.xAxis(), "max"), QString("2.5"));
}

TEST_F(PropertyInspectorTest, TextThatIsNoValueIsRefused)
{
    const double before = m_plot.xAxis()->max();

    EXPECT_FALSE(m_inspector.setValue(row(m_plot.xAxis(), "max"), QString("wide")));
    EXPECT_FALSE(m_inspector.setValue(row(m_line, "color"), QString("not a color")));
    EXPECT_FALSE(m_inspector.setValue(row(m_plot.xAxis(), "scaleType"), QString("CUBIC")));
    EXPECT_FALSE(m_inspector.setValue(row(m_plot.legend(), "position"), QString("1")));
    EXPECT_FALSE(m_inspector.setValue(row(m_plot.xAxis(), "timeZone"), QString("Mars/Olympus")));

    EXPECT_DOUBLE_EQ(m_plot.xAxis()->max(), before);
    EXPECT_EQ(m_plot.xAxis()->scaleType(), rocketplot::ScaleType::LINEAR);
}

TEST_F(PropertyInspectorTest, AValueTheSetterRefusesIsNotShown)
{
    // A line can't be -3 pixels wide: the row goes on showing the width the line has.
    const QString before = shown(m_line, "lineWidth");

    m_inspector.setValue(row(m_line, "lineWidth"), QString("-3"));

    EXPECT_EQ(shown(m_line, "lineWidth"), before);
    EXPECT_GT(m_line->lineWidth(), 0.0);
}

TEST_F(PropertyInspectorTest, TextSetsTheOtherKindsToo)
{
    EXPECT_TRUE(m_inspector.setValue(row(&m_plot, "title"), QString("Descent")));
    EXPECT_TRUE(m_inspector.setValue(row(m_plot.yAxis(), "scaleType"), QString("LOGARITHMIC")));
    EXPECT_TRUE(m_inspector.setValue(row(m_line, "lineStyle"), QString("DashLine")));
    EXPECT_TRUE(m_inspector.setValue(row(m_line, "color"), QString("#ff8800")));
    EXPECT_TRUE(m_inspector.setValue(row(m_plot.legend(), "position"), QString("0.25, 0.5")));
    EXPECT_TRUE(m_inspector.setValue(row(m_limit, "labelAlignment"), QString("Left | Bottom")));
    EXPECT_TRUE(m_inspector.setValue(row(m_plot.xAxis(), "timeZone"), QString("UTC+02:00")));

    EXPECT_EQ(m_plot.title(), QString("Descent"));
    EXPECT_EQ(m_plot.yAxis()->scaleType(), rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_EQ(m_line->lineStyle(), Qt::DashLine);
    EXPECT_EQ(m_line->color(), QColor(255, 136, 0));
    EXPECT_EQ(m_plot.legend()->position(), QPointF(0.25, 0.5));
    EXPECT_EQ(shown(m_plot.legend(), "anchor"), QString("CUSTOM"));  // what setting it did
    EXPECT_EQ(m_limit->labelAlignment(), Qt::AlignLeft | Qt::AlignBottom);
    EXPECT_EQ(m_plot.xAxis()->timeZone(), QTimeZone(QByteArrayLiteral("UTC+02:00")));
}

TEST_F(PropertyInspectorTest, ACheckboxSetsAFlag)
{
    row(m_plot.xAxis(), "gridVisible")->setCheckState(kValue, Qt::Unchecked);
    row(m_line, "visible")->setCheckState(kValue, Qt::Unchecked);
    row(&m_plot, "crosshairEnabled")->setCheckState(kValue, Qt::Checked);

    EXPECT_FALSE(m_plot.xAxis()->isGridVisible());
    EXPECT_FALSE(m_line->isVisible());
    EXPECT_TRUE(m_plot.isCrosshairEnabled());
}

TEST_F(PropertyInspectorTest, ResetHandsAPropertyBackToTheTheme)
{
    m_line->setColor(Qt::red);
    m_line->setLineWidth(9.0);
    settle();

    m_inspector.resetValue(row(m_line, "color"));
    m_inspector.resetValue(row(m_line, "lineWidth"));

    EXPECT_EQ(m_line->color(), rocketplot::Theme::light().seriesColor(0));
    EXPECT_DOUBLE_EQ(m_line->lineWidth(), rocketplot::Theme::light().lineWidth);
    EXPECT_EQ(shown(m_line, "color"), rocketplot::Theme::light().seriesColor(0).name());
}

TEST_F(PropertyInspectorTest, WhatCannotBeSetIsNotEdited)
{
    QTreeWidgetItem* orientation = row(m_limit, "orientation");

    EXPECT_FALSE(orientation->flags().testFlag(Qt::ItemIsEditable));
    EXPECT_FALSE(m_inspector.setValue(orientation, QString("Vertical")));
    EXPECT_EQ(m_limit->orientation(), Qt::Horizontal);
    EXPECT_TRUE(row(m_limit, "value")->flags().testFlag(Qt::ItemIsEditable));
}

TEST_F(PropertyInspectorTest, TypingInARowSetsTheProperty)
{
    m_inspector.editItem(row(m_plot.xAxis(), "max"), kValue);
    auto* editor = m_inspector.findChild<QLineEdit*>();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->text(), shown(m_plot.xAxis(), "max"));

    editor->setText("42");
    QTest::keyClick(editor, Qt::Key_Return);
    settle();  // the view takes the editor's value once the editor has seen the key

    EXPECT_DOUBLE_EQ(m_plot.xAxis()->max(), 42.0);
}

TEST_F(PropertyInspectorTest, ChoosingFromARowsListSetsTheProperty)
{
    m_inspector.editItem(row(m_plot.xAxis(), "numberFormat"), kValue);
    auto* editor = m_inspector.findChild<QComboBox*>();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->currentText(), QString("AUTO"));
    const int choice = editor->findText("SI");
    ASSERT_GE(choice, 0);

    editor->setCurrentIndex(choice);
    Q_EMIT editor->activated(choice);  // what picking it with the mouse does

    EXPECT_EQ(m_plot.xAxis()->numberFormat(), rocketplot::NumberFormat::SI);
}

TEST_F(PropertyInspectorTest, ALineStyleListOffersTheStylesThereAre)
{
    m_inspector.editItem(row(m_line, "lineStyle"), kValue);
    const auto* editor = m_inspector.findChild<QComboBox*>();
    ASSERT_NE(editor, nullptr);

    EXPECT_GE(editor->findText("DashDotLine"), 0);
    EXPECT_LT(editor->findText("MPenStyle"), 0);  // Qt's end marker: not a style
}

TEST_F(PropertyInspectorTest, SeriesAndAnnotationsThatComeLaterAreListed)
{
    const LineSeries* added = m_plot.addLine(std::vector<double>{0.0}, std::vector<double>{1.0});
    const auto*       event = m_plot.addEvent(0.5, "MECO");
    settle();

    ASSERT_NE(m_inspector.objectItem(added), nullptr);
    EXPECT_EQ(m_inspector.objectItem(added)->text(0), QString("Series 2 (LineSeries)"));
    ASSERT_NE(m_inspector.objectItem(event), nullptr);
    EXPECT_EQ(m_inspector.objectItem(event)->text(0), QString("EventMarker: MECO"));
    EXPECT_NE(m_inspector.propertyItem(m_line, "name"), nullptr);  // the old ones are still there
}

TEST_F(PropertyInspectorTest, WhatIsRemovedIsNoLongerListed)
{
    m_plot.removeSeries(m_line);
    m_plot.clearAnnotations();
    settle();

    EXPECT_EQ(m_inspector.objectItem(m_line), nullptr);
    EXPECT_EQ(m_inspector.propertyItem(m_limit, "value"), nullptr);
    EXPECT_NE(m_inspector.propertyItem(&m_plot, "title"), nullptr);
}

TEST_F(PropertyInspectorTest, ASinglePlotStartsWithItsAxesOpen)
{
    EXPECT_TRUE(m_inspector.objectItem(&m_plot)->isExpanded());
    EXPECT_TRUE(m_inspector.objectItem(m_plot.xAxis())->isExpanded());
    EXPECT_TRUE(m_inspector.objectItem(m_plot.yAxis())->isExpanded());
    EXPECT_FALSE(m_inspector.objectItem(m_plot.yAxis2())->isExpanded());
    EXPECT_FALSE(m_inspector.objectItem(m_line)->isExpanded());

    PlotWidget other;
    m_inspector.setPlots({&m_plot, &other});  // several: only their own rows

    EXPECT_TRUE(m_inspector.objectItem(&other)->isExpanded());
    EXPECT_FALSE(m_inspector.objectItem(m_plot.xAxis())->isExpanded());
    m_inspector.setPlots({});
}

TEST_F(PropertyInspectorTest, RowsStayAsTheyAreWhenTheListIsMadeAnew)
{
    m_inspector.objectItem(m_plot.legend())->setExpanded(true);
    m_inspector.objectItem(m_plot.yAxis())->setExpanded(false);

    m_plot.addLine(std::vector<double>{0.0}, std::vector<double>{1.0});
    settle();

    EXPECT_TRUE(m_inspector.objectItem(m_plot.legend())->isExpanded());
    EXPECT_FALSE(m_inspector.objectItem(m_plot.yAxis())->isExpanded());
    EXPECT_TRUE(m_inspector.objectItem(m_plot.xAxis())->isExpanded());
    EXPECT_TRUE(m_inspector.objectItem(&m_plot)->isExpanded());
}

TEST_F(PropertyInspectorTest, APlotThatIsDeletedLeavesNoRows)
{
    auto doomed = std::make_unique<PlotWidget>();
    m_inspector.setPlots({doomed.get(), &m_plot});
    ASSERT_EQ(m_inspector.topLevelItemCount(), 2);

    doomed.reset();
    settle();

    EXPECT_EQ(m_inspector.topLevelItemCount(), 1);
    EXPECT_NE(m_inspector.propertyItem(&m_plot, "title"), nullptr);
}

TEST_F(PropertyInspectorTest, ShowsNothingWithoutPlots)
{
    m_inspector.setPlots({});

    EXPECT_EQ(m_inspector.topLevelItemCount(), 0);
    EXPECT_EQ(m_inspector.propertyItem(&m_plot, "title"), nullptr);
    m_plot.setTitle("Still fine");  // no longer watched
    settle();
}

}  // namespace
