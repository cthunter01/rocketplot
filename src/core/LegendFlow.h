#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace rocketplot::core
{

/// How the entries of a legend are set out in a grid of columns, each as wide as its widest
/// entry.
struct LegendGrid
{
    std::vector<double> columnWidths;
    std::size_t         rows = 0;
    /// Whether the entries run across a row before the next row starts (else down a column).
    bool byRows = true;

    [[nodiscard]] std::size_t columns() const noexcept { return columnWidths.size(); }
    /// The column and the row of entry @p entry.
    [[nodiscard]] std::size_t columnOf(std::size_t entry) const noexcept
    {
        return byRows ? entry % columns() : entry / rows;
    }
    [[nodiscard]] std::size_t rowOf(std::size_t entry) const noexcept
    {
        return byRows ? entry / columns() : entry % rows;
    }
    /// The columns' widths with @p gap between neighbors.
    [[nodiscard]] double width(double gap) const noexcept;
};

/// Entries @p widths wide set out across rows no wider than @p available: as many columns as fit
/// (at least one), each as wide as its widest entry, entries in order along each row.
[[nodiscard]] LegendGrid flowAcross(std::span<const double> widths, double available, double gap);

/// Entries @p widths wide set out down columns of @p rowsPerColumn entries (at least one), each
/// column as wide as its widest entry.
[[nodiscard]] LegendGrid flowDown(std::span<const double> widths, std::size_t rowsPerColumn);

}  // namespace rocketplot::core
