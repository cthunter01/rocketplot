#include "DelimitedText.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QTimeZone>
#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::demo::DataTable;
using rocketplot::demo::parseDelimitedText;

// The values of a column, with -1 standing in for a gap (NaN compares unequal to itself).
std::vector<double> valuesOf(const DataTable& table, std::size_t column)
{
    std::vector<double> values = table.columns.at(column).values;
    for (double& value : values)
    {
        value = std::isnan(value) ? -1.0 : value;
    }
    return values;
}

TEST(DelimitedText, ReadsNamedColumnsSeparatedByCommas)
{
    const DataTable table = parseDelimitedText("t,alt,vel\n0,1.5,20\n1,2.5,30\n");

    EXPECT_EQ(table.separator, ',');
    EXPECT_TRUE(table.hasHeader);
    EXPECT_EQ(table.rows, 2U);
    ASSERT_EQ(table.columns.size(), 3U);
    EXPECT_EQ(table.columns[1].name, QString("alt"));
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{0.0, 1.0}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{1.5, 2.5}));
    EXPECT_EQ(valuesOf(table, 2), (std::vector<double>{20.0, 30.0}));
    EXPECT_FALSE(table.isEmpty());
}

TEST(DelimitedText, NumbersColumnsThatHaveNoNames)
{
    const DataTable table = parseDelimitedText("0,1\n2,3\n");

    EXPECT_FALSE(table.hasHeader);
    EXPECT_EQ(table.rows, 2U);
    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(table.columns[0].name, QString("Column 1"));
    EXPECT_EQ(table.columns[1].name, QString("Column 2"));
}

TEST(DelimitedText, ReadsCellsCopiedFromASpreadsheet)
{
    const DataTable table = parseDelimitedText("Time (s)\tAltitude (km)\n0\t1\n10\t2\n");

    EXPECT_EQ(table.separator, '\t');
    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(table.columns[0].name, QString("Time (s)"));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{1.0, 2.0}));
}

TEST(DelimitedText, ReadsSemicolonsWithDecimalCommas)
{
    const DataTable table = parseDelimitedText("t;v\n0,5;1,25\n1,5;2\n");

    EXPECT_EQ(table.separator, ';');
    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{0.5, 1.5}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{1.25, 2.0}));
}

TEST(DelimitedText, ReadsColumnsSeparatedBySpaces)
{
    const DataTable table = parseDelimitedText("  1   2.5  3\n 4 5\t6 \n");

    EXPECT_EQ(table.separator, ' ');
    ASSERT_EQ(table.columns.size(), 3U);
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{2.5, 5.0}));
    EXPECT_EQ(valuesOf(table, 2), (std::vector<double>{3.0, 6.0}));
}

TEST(DelimitedText, ReadsASingleColumn)
{
    const DataTable table = parseDelimitedText("volts\n1\n2\n3");

    ASSERT_EQ(table.columns.size(), 1U);
    EXPECT_EQ(table.columns[0].name, QString("volts"));
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{1.0, 2.0, 3.0}));
}

TEST(DelimitedText, QuotesKeepSeparatorsAndQuotesInAField)
{
    const DataTable table =
        parseDelimitedText("\"Altitude, km\",\"The \"\"good\"\" one\"\n\"1.5\", \"2\"\n");

    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(table.columns[0].name, QString("Altitude, km"));
    EXPECT_EQ(table.columns[1].name, QString("The \"good\" one"));
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{1.5}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{2.0}));
}

TEST(DelimitedText, CellsWithoutANumberAreGaps)
{
    const DataTable table = parseDelimitedText("a,b\n1,\n,2\nn/a,3\nNaN,inf\n");

    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{1.0, -1.0, -1.0, -1.0}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{-1.0, 2.0, 3.0, -1.0}));
    EXPECT_EQ(table.columns[0].numbers, 1U);
    EXPECT_EQ(table.columns[1].numbers, 2U);
}

TEST(DelimitedText, ReadsNumbersHoweverTheyAreWritten)
{
    const DataTable table = parseDelimitedText("1e3, -2.5E-2 ,+4,.5\n");

    ASSERT_EQ(table.columns.size(), 4U);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{1000.0}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{-0.025}));
    EXPECT_EQ(valuesOf(table, 2), (std::vector<double>{4.0}));
    EXPECT_EQ(valuesOf(table, 3), (std::vector<double>{0.5}));
}

TEST(DelimitedText, RowsMayDifferInLength)
{
    const DataTable table = parseDelimitedText("1,2,3\n4\n5,6,7,8\n");

    EXPECT_EQ(table.rows, 3U);
    ASSERT_EQ(table.columns.size(), 4U);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{1.0, 4.0, 5.0}));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{2.0, -1.0, 6.0}));
    EXPECT_EQ(valuesOf(table, 3), (std::vector<double>{-1.0, -1.0, 8.0}));
    EXPECT_EQ(table.columns[3].name, QString("Column 4"));
}

TEST(DelimitedText, SkipsCommentsAndEmptyLines)
{
    const DataTable table = parseDelimitedText("# logged 2026-10-03\n\nt,v\n  # mid\n1,2\n\n3,4\n");

    EXPECT_TRUE(table.hasHeader);
    EXPECT_EQ(table.rows, 2U);
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{2.0, 4.0}));
}

TEST(DelimitedText, ReadsWindowsLineEndsAndAByteOrderMark)
{
    const DataTable table = parseDelimitedText("\xEF\xBB\xBFt,v\r\n1,2\r\n");

    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(table.columns[0].name, QString("t"));
    EXPECT_EQ(table.columns[1].name, QString("v"));
    EXPECT_EQ(valuesOf(table, 1), (std::vector<double>{2.0}));
}

TEST(DelimitedText, NamesMayHoldAnyLetters)
{
    const DataTable table = parseDelimitedText("Zeit,Temperatur (°C)\n0,21.5\n");

    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_EQ(table.columns[1].name, QString::fromUtf8("Temperatur (°C)"));
}

TEST(DelimitedText, ReadsDatesAndTimesAsSecondsSinceTheEpoch)
{
    const DataTable table = parseDelimitedText(
        "when,v\n"
        "2026-10-03T12:00:00Z,1\n"
        "2026-10-03 12:00:01.500,2\n"
        "2026-10-03T14:00:02+02:00,3\n");

    const auto noon = static_cast<double>(
        QDateTime(QDate(2026, 10, 3), QTime(12, 0), QTimeZone::UTC).toSecsSinceEpoch());
    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_TRUE(table.columns[0].isTime);
    EXPECT_FALSE(table.columns[1].isTime);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{noon, noon + 1.5, noon + 2.0}));
}

TEST(DelimitedText, ADateAloneIsItsMidnight)
{
    const DataTable table = parseDelimitedText("2026-10-03,5\n2026-10-04,6\n");

    const auto midnight = static_cast<double>(
        QDateTime(QDate(2026, 10, 3), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch());
    EXPECT_FALSE(table.hasHeader);
    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_TRUE(table.columns[0].isTime);
    EXPECT_EQ(valuesOf(table, 0), (std::vector<double>{midnight, midnight + 86400.0}));
}

TEST(DelimitedText, TextWithoutNumbersIsAnEmptyTable)
{
    EXPECT_TRUE(parseDelimitedText("").isEmpty());
    EXPECT_TRUE(parseDelimitedText("\n\n# nothing\n").isEmpty());
    EXPECT_TRUE(parseDelimitedText("name,place\nAda,London\n").isEmpty());
    EXPECT_TRUE(parseDelimitedText("only,names\n").isEmpty());
}

TEST(DelimitedText, ATextColumnBesideNumbersIsKeptButNotNumeric)
{
    const DataTable table = parseDelimitedText("t,phase\n0,ascent\n1,coast\n");

    ASSERT_EQ(table.columns.size(), 2U);
    EXPECT_TRUE(table.columns[0].isNumeric());
    EXPECT_FALSE(table.columns[1].isNumeric());
    EXPECT_FALSE(table.isEmpty());
}

TEST(DelimitedText, ReadsALargeTable)
{
    QByteArray text("t,a,b\n");
    for (int row = 0; row < 100'000; ++row)
    {
        text += QByteArray::number(row) + ',' + QByteArray::number(row * 0.5) + ',' +
                QByteArray::number(-row) + '\n';
    }

    const DataTable table = parseDelimitedText(text);

    EXPECT_EQ(table.rows, 100'000U);
    ASSERT_EQ(table.columns.size(), 3U);
    EXPECT_EQ(table.columns[1].numbers, 100'000U);
    EXPECT_DOUBLE_EQ(table.columns[1].values.back(), 49'999.5);
    EXPECT_DOUBLE_EQ(table.columns[2].values.back(), -99'999.0);
}

}  // namespace
