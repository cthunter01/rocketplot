#include "core/LegendFlow.h"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace rocketplot::core
{

namespace
{

// The columns' widths of @p widths set out as @p grid says (its columns and rows already set).
void fillWidths(LegendGrid& grid, std::span<const double> widths)
{
    for (std::size_t i = 0; i < widths.size(); ++i)
    {
        double& column = grid.columnWidths[grid.columnOf(i)];
        column         = std::max(column, widths[i]);
    }
}

}  // namespace

double LegendGrid::width(double gap) const noexcept
{
    if (columnWidths.empty())
    {
        return 0.0;
    }
    return std::accumulate(columnWidths.begin(), columnWidths.end(), 0.0) +
           (gap * static_cast<double>(columnWidths.size() - 1));
}

LegendGrid flowAcross(std::span<const double> widths, double available, double gap)
{
    LegendGrid grid;
    if (widths.empty())
    {
        return grid;
    }
    // The most columns that fit: fewer columns are never narrower in all, so the first count that
    // fits from the most down is it.
    for (std::size_t columns = widths.size(); columns >= 1; --columns)
    {
        grid.columnWidths.assign(columns, 0.0);
        grid.rows   = (widths.size() + columns - 1) / columns;
        grid.byRows = true;
        fillWidths(grid, widths);
        if (columns == 1 || grid.width(gap) <= available)
        {
            break;
        }
    }
    return grid;
}

LegendGrid flowDown(std::span<const double> widths, std::size_t rowsPerColumn)
{
    LegendGrid grid;
    if (widths.empty())
    {
        return grid;
    }
    grid.rows   = std::clamp<std::size_t>(rowsPerColumn, 1, widths.size());
    grid.byRows = false;
    grid.columnWidths.assign((widths.size() + grid.rows - 1) / grid.rows, 0.0);
    fillWidths(grid, widths);
    return grid;
}

}  // namespace rocketplot::core
