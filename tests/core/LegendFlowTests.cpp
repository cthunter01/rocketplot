#include "core/LegendFlow.h"

#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::flowAcross;
using rocketplot::core::flowDown;
using rocketplot::core::LegendGrid;

TEST(LegendFlow, AcrossAsManyColumnsAsFit)
{
    const std::vector<double> widths{50, 80, 30, 60, 40};
    // All in one row when there is room.
    LegendGrid grid = flowAcross(widths, 1000.0, 10.0);
    EXPECT_EQ(grid.columns(), 5U);
    EXPECT_EQ(grid.rows, 1U);
    EXPECT_DOUBLE_EQ(grid.width(10.0), 300.0);
    // Two columns: 50 30 40 and 80 60, as wide as their widest.
    grid = flowAcross(widths, 160.0, 10.0);
    ASSERT_EQ(grid.columns(), 2U);
    EXPECT_EQ(grid.rows, 3U);
    EXPECT_DOUBLE_EQ(grid.columnWidths[0], 50.0);
    EXPECT_DOUBLE_EQ(grid.columnWidths[1], 80.0);
    EXPECT_EQ(grid.columnOf(3), 1U);
    EXPECT_EQ(grid.rowOf(3), 1U);
    // At least one column, however narrow the room.
    grid = flowAcross(widths, 10.0, 10.0);
    EXPECT_EQ(grid.columns(), 1U);
    EXPECT_EQ(grid.rows, 5U);
    EXPECT_DOUBLE_EQ(grid.width(10.0), 80.0);
}

TEST(LegendFlow, DownColumnsOfSoManyRows)
{
    const std::vector<double> widths{50, 80, 30, 60, 40};
    LegendGrid                grid = flowDown(widths, 2);
    ASSERT_EQ(grid.columns(), 3U);
    EXPECT_EQ(grid.rows, 2U);
    EXPECT_DOUBLE_EQ(grid.columnWidths[0], 80.0);
    EXPECT_DOUBLE_EQ(grid.columnWidths[1], 60.0);
    EXPECT_DOUBLE_EQ(grid.columnWidths[2], 40.0);
    EXPECT_EQ(grid.columnOf(3), 1U);
    EXPECT_EQ(grid.rowOf(3), 1U);
    // One column when the rows are enough; at least one row.
    EXPECT_EQ(flowDown(widths, 10).columns(), 1U);
    EXPECT_EQ(flowDown(widths, 0).columns(), 5U);
}

TEST(LegendFlow, NothingForNoEntries)
{
    EXPECT_EQ(flowAcross({}, 100.0, 10.0).columns(), 0U);
    EXPECT_EQ(flowDown({}, 3).columns(), 0U);
    EXPECT_DOUBLE_EQ(LegendGrid{}.width(10.0), 0.0);
}

}  // namespace
