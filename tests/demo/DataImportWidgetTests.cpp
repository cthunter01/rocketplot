#include "DataImportWidget.h"

#include <QByteArrayView>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QMimeData>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::Series;
using rocketplot::demo::DataImportWidget;

constexpr QByteArrayView kTable("t,a,b\n0,10,-1\n1,20,-2\n2,30,-3\n");

// Writes @p text to a file in @p directory and returns its path.
QString fileWith(const QTemporaryDir& directory, const QString& name, QByteArrayView text)
{
    const QString path = directory.filePath(name);
    QFile         file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(text.data(), text.size());
    return path;
}

class DataImportWidgetTest : public testing::Test
{
protected:
    void SetUp() override { m_importer.resize(900, 500); }

    [[nodiscard]] QList<Series*> series() const { return m_importer.plot()->series(); }

    DataImportWidget m_importer;
};

TEST_F(DataImportWidgetTest, PlotsAColumnAgainstTheFirstWhenThatIsATimeBase)
{
    ASSERT_TRUE(m_importer.loadText(kTable, "test"));

    EXPECT_EQ(m_importer.xColumn(), 0);
    ASSERT_EQ(series().size(), 2);
    EXPECT_EQ(series().at(0)->name(), QString("a"));
    EXPECT_EQ(series().at(1)->name(), QString("b"));
    EXPECT_DOUBLE_EQ(series().at(0)->x(2), 2.0);
    EXPECT_DOUBLE_EQ(series().at(0)->y(2), 30.0);
    EXPECT_DOUBLE_EQ(series().at(1)->y(1), -2.0);
    EXPECT_EQ(m_importer.plot()->xAxis()->label(), QString("t"));
    EXPECT_EQ(m_importer.plot()->title(), QString("test"));
    EXPECT_TRUE(m_importer.status().contains("3 rows"));
}

TEST_F(DataImportWidgetTest, PlotsAgainstTheRowNumberWithoutATimeBase)
{
    ASSERT_TRUE(m_importer.loadText("5,1\n3,2\n4,3\n", "test"));

    EXPECT_EQ(m_importer.xColumn(), -1);
    ASSERT_EQ(series().size(), 2);
    EXPECT_TRUE(series().at(0)->hasUniformX());
    EXPECT_DOUBLE_EQ(series().at(0)->x(2), 2.0);
    EXPECT_DOUBLE_EQ(series().at(0)->y(2), 4.0);
}

TEST_F(DataImportWidgetTest, TimesMakeATimeAxis)
{
    ASSERT_TRUE(
        m_importer.loadText("v,when\n1,2026-10-03T12:00:00Z\n2,2026-10-03T12:00:01Z\n", "test"));

    EXPECT_EQ(m_importer.xColumn(), 1);
    EXPECT_EQ(m_importer.plot()->xAxis()->scaleType(), rocketplot::ScaleType::DATE_TIME);
    ASSERT_EQ(series().size(), 1);
    EXPECT_EQ(series().at(0)->name(), QString("v"));

    ASSERT_TRUE(m_importer.loadText(kTable, "test"));
    EXPECT_EQ(m_importer.plot()->xAxis()->scaleType(), rocketplot::ScaleType::LINEAR);
}

TEST_F(DataImportWidgetTest, TextWithoutNumbersLeavesThePlotAsItWas)
{
    ASSERT_TRUE(m_importer.loadText(kTable, "test"));

    EXPECT_FALSE(m_importer.loadText("name,place\nAda,London\n", "people"));

    EXPECT_EQ(series().size(), 2);
    EXPECT_EQ(m_importer.table().rows, 3U);
    EXPECT_TRUE(m_importer.status().contains("people"));
}

TEST_F(DataImportWidgetTest, ColumnsOfTextAreNotOffered)
{
    ASSERT_TRUE(m_importer.loadText("t,phase,v\n0,ascent,1\n1,coast,2\n", "test"));

    ASSERT_EQ(series().size(), 1);
    EXPECT_EQ(series().at(0)->name(), QString("v"));
}

TEST_F(DataImportWidgetTest, AColumnCanBeTakenOutAndPutBack)
{
    ASSERT_TRUE(m_importer.loadText(kTable, "test"));
    EXPECT_TRUE(m_importer.isPlotted(1));

    m_importer.setPlotted(1, false);
    EXPECT_FALSE(m_importer.isPlotted(1));
    EXPECT_FALSE(series().at(0)->isVisible());
    EXPECT_TRUE(m_importer.isPlotted(2));

    m_importer.setPlotted(1, true);
    EXPECT_TRUE(series().at(0)->isVisible());
    EXPECT_FALSE(m_importer.isPlotted(0));  // the x column is not a series
    EXPECT_FALSE(m_importer.isPlotted(7));
}

TEST_F(DataImportWidgetTest, AnotherXColumnDrawsTheOthersAgainstIt)
{
    ASSERT_TRUE(m_importer.loadText(kTable, "test"));

    m_importer.setXColumn(1);

    EXPECT_EQ(m_importer.xColumn(), 1);
    ASSERT_EQ(series().size(), 2);
    EXPECT_EQ(series().at(0)->name(), QString("t"));
    EXPECT_DOUBLE_EQ(series().at(0)->x(1), 20.0);
    EXPECT_EQ(m_importer.plot()->xAxis()->label(), QString("a"));

    m_importer.setXColumn(-1);
    EXPECT_EQ(series().size(), 3);
    EXPECT_EQ(m_importer.plot()->xAxis()->label(), QString("Row"));
}

TEST_F(DataImportWidgetTest, AWideTableStartsWithItsFirstColumnsDrawn)
{
    ASSERT_TRUE(m_importer.loadText("0,1,2,3,4,5,6,7,8,9\n1,1,2,3,4,5,6,7,8,9\n", "test"));

    ASSERT_EQ(series().size(), 9);
    EXPECT_TRUE(m_importer.isPlotted(6));
    EXPECT_FALSE(m_importer.isPlotted(7));
    EXPECT_FALSE(m_importer.isPlotted(9));
}

TEST_F(DataImportWidgetTest, ReadsAFile)
{
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    ASSERT_TRUE(m_importer.loadFile(fileWith(directory, "flight.csv", kTable)));
    EXPECT_EQ(m_importer.plot()->title(), QString("flight.csv"));
    EXPECT_EQ(series().size(), 2);

    EXPECT_FALSE(m_importer.loadFile(directory.filePath("missing.csv")));
    EXPECT_TRUE(m_importer.status().contains("missing.csv"));
    EXPECT_EQ(series().size(), 2);
}

TEST_F(DataImportWidgetTest, ReadsWhatIsDroppedOrPasted)
{
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QMimeData text;
    text.setText("x\ty\n1\t2\n2\t4\n");
    QMimeData file;
    file.setUrls({QUrl::fromLocalFile(fileWith(directory, "dropped.csv", kTable))});
    const QMimeData nothing;

    EXPECT_TRUE(DataImportWidget::canRead(&text));
    EXPECT_TRUE(DataImportWidget::canRead(&file));
    EXPECT_FALSE(DataImportWidget::canRead(&nothing));
    EXPECT_FALSE(DataImportWidget::canRead(nullptr));

    ASSERT_TRUE(m_importer.loadMimeData(&text, "Clipboard"));
    EXPECT_EQ(m_importer.plot()->title(), QString("Clipboard"));
    EXPECT_EQ(m_importer.table().separator, '\t');
    ASSERT_TRUE(m_importer.loadMimeData(&file, "Clipboard"));
    EXPECT_EQ(m_importer.plot()->title(), QString("dropped.csv"));
    EXPECT_FALSE(m_importer.loadMimeData(&nothing, "Clipboard"));
    EXPECT_EQ(m_importer.plot()->title(), QString("dropped.csv"));
}

TEST_F(DataImportWidgetTest, HidingASeriesInTheLegendTakesItsColumnOut)
{
    ASSERT_TRUE(m_importer.loadText(kTable, "test"));

    series().at(1)->setVisible(false);  // what a click on its legend entry does

    EXPECT_FALSE(m_importer.isPlotted(2));
    m_importer.setPlotted(2, true);
    EXPECT_TRUE(series().at(1)->isVisible());
}

}  // namespace
