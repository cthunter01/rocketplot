#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Decimator.h"

namespace rocketplot::core
{

/// A coarse picture of where a plot's data is drawn (one cell per few pixels), recorded as the
/// series are drawn, so the legend can go where it covers the least data.
class Occupancy
{
public:
    /// Starts an empty picture of @p area (the plot area, in pixels).
    void reset(PixelBox area);
    /// Forgets everything: isRecorded() is false until the next reset().
    void               clear();
    [[nodiscard]] bool isRecorded() const noexcept { return m_columns > 0; }

    /// Records a line @p width pixels wide along the runs of @p line.
    void addPolyline(const Polyline& line, double width);
    /// Records markers @p size pixels across at @p points.
    void addPoints(const std::vector<PixelPoint>& points, double size);
    /// Records everything inside @p box (a label, an error bar, one marker).
    void addBox(PixelBox box);

    /// How much of @p box data covers, from 0 (none) to 1 (all of it).
    [[nodiscard]] double coverage(PixelBox box) const;

private:
    [[nodiscard]] std::size_t columnOf(double x) const;
    [[nodiscard]] std::size_t rowOf(double y) const;
    // Marks the cells of @p column from y = top to bottom.
    void markColumn(std::size_t column, double top, double bottom);
    void sum() const;

    PixelBox                  m_area;
    std::size_t               m_columns = 0;
    std::size_t               m_rows    = 0;
    std::vector<std::uint8_t> m_cells;  // row by row; nonzero where something is drawn
    // Summed-area table: (columns + 1) x (rows + 1) counts of the occupied cells above and left of
    // each corner. Built on the first coverage() after a change.
    mutable std::vector<std::uint32_t> m_sums;
    mutable bool                       m_summed = false;
};

}  // namespace rocketplot::core
