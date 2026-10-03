#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

#include "core/Decimator.h"

namespace rocketplot::core
{

/// The pixel rows a line covers in one pixel column (top <= bottom; empty when top > bottom).
struct ColumnSpan
{
    double top    = std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();

    [[nodiscard]] bool empty() const noexcept { return top > bottom; }
};

/// Pixel columns: @p count columns of @p width pixels, the first starting at @p left.
struct ColumnGrid
{
    double      left  = 0.0;
    double      width = 1.0;
    std::size_t count = 0;

    /// The left edge and the center of a column.
    [[nodiscard]] double edge(std::size_t column) const noexcept
    {
        return left + (static_cast<double>(column) * width);
    }
    [[nodiscard]] double center(std::size_t column) const noexcept
    {
        return edge(column) + (width / 2.0);
    }
    /// The column containing @p x; the first or last one for an x beside the grid.
    [[nodiscard]] std::size_t columnAt(double x) const noexcept;
};

/// Writes the outline of what covers columns [begin, end) of @p spans (none of them empty) to
/// @p polygons: closed polygons (Polyline runs) of up to 16 columns each, which QPainter fills
/// much faster than one polygon over all of them. Each runs along the tops through the column
/// centers and back along the bottoms, so slanted edges stay antialiased; neighboring pieces meet
/// at a column's edge, without a seam.
void outlineColumns(const ColumnGrid& grid, std::span<const ColumnSpan> spans, std::size_t begin,
                    std::size_t end, Polyline& polygons);

/// Draws thick lines through dense data as filled outlines instead of strokes.
///
/// Stroking a 2-pixel line that zigzags up and down in every pixel column (what min/max decimation
/// produces) is very slow in QPainter: the stroke outline overlaps itself thousands of times. But
/// in every column, such a line simply covers one vertical stretch. This class computes that
/// stretch per column exactly, for a round pen of half-width @p halfWidth (the union of the
/// "capsules" around each segment, sampled at each column's center), and turns it into one simple
/// polygon per run, which fills in a fraction of the time and looks the same.
///
/// Only for runs whose x never decreases (sorted data); a run's polygon follows its column centers,
/// so slanted edges stay antialiased.
class LineBand
{
public:
    /// Writes the outline of each run of @p line into @p polygons, as closed polygons (Polyline
    /// runs) of up to 16 columns each.
    void outline(const Polyline& line, const ColumnGrid& grid, double halfWidth,
                 Polyline& polygons);

    /// The covered span of each column after the last run outline() processed (for tests).
    [[nodiscard]] std::span<const ColumnSpan> spans() const noexcept { return m_spans; }

private:
    void cover(PixelPoint a, PixelPoint b, const ColumnGrid& grid, double halfWidth);

    std::vector<ColumnSpan> m_spans;
};

}  // namespace rocketplot::core
