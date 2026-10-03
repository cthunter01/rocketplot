#include "core/ErrorGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/ErrorData.h"
#include "core/LineBand.h"
#include "core/MinMaxPyramid.h"
#include "core/SeriesData.h"

namespace rocketplot::core
{

namespace
{

// Where an end that an axis can't place is put: far outside the plot, for clipping to cut off.
constexpr double kFarPixels = 1e6;
// The grid that keeps one bar per cell is capped at this many cells (8 MB); larger boxes use
// coarser cells.
constexpr double kMaxBarCells = 64.0 * 1024.0 * 1024.0;
// Half the width of the band at a lone point between gaps, so it shows (a line draws a dot there).
constexpr double kLonePointHalfWidth = 2.0;

// +1 where pixels grow toward the axis's high values, -1 where they shrink (a y axis).
double directionOf(const AxisMapping& axis)
{
    return axis.pixelEnd() >= axis.pixelStart() ? 1.0 : -1.0;
}

// The pixel of a bar's low end: far beyond the low end of the axis where the axis can't show the
// value (<= 0 on a log axis, or -infinity). NaN for a NaN value.
double lowEndPixel(const AxisMapping& axis, double value)
{
    const double pixel = axis.toPixel(value);
    if (std::isfinite(pixel) || std::isnan(value))
    {
        return pixel;
    }
    return axis.pixelStart() - (directionOf(axis) * kFarPixels);
}

// The pixel of a bar's high end: far beyond the high end of the axis for +infinity, and NaN where
// the axis can't show the value at all (then it can't show the point either).
double highEndPixel(const AxisMapping& axis, double value)
{
    const double pixel = axis.toPixel(value);
    if (std::isfinite(pixel))
    {
        return pixel;
    }
    if (value == std::numeric_limits<double>::infinity())
    {
        return axis.pixelEnd() + (directionOf(axis) * kFarPixels);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// Where a band's edges are at one x.
struct BandEdge
{
    double x      = 0.0;
    double top    = 0.0;
    double bottom = 0.0;
};

// Collects what a band covers in each pixel column, from its edges given left to right, and writes
// the outline of each stretch between gaps.
class BandBuilder
{
public:
    BandBuilder(const ColumnGrid& grid, double clipTop, double clipBottom, Polyline& out)
      : m_grid(grid), m_clipTop(clipTop), m_clipBottom(clipBottom), m_out(&out), m_spans(grid.count)
    {
    }

    // The band runs from the edge added before to this one (x never decreases).
    void add(double x, double top, double bottom)
    {
        const BandEdge edge{.x = x, .top = top, .bottom = bottom};
        if (m_edges > 0)
        {
            cover(m_last, edge);
        }
        m_last = edge;
        ++m_edges;
    }

    // Ends the stretch: the next add() starts a new outline.
    void gap()
    {
        if (m_edges == 1)
        {
            // A lone point: a sliver, so it shows.
            BandEdge left  = m_last;
            BandEdge right = m_last;
            left.x -= kLonePointHalfWidth;
            right.x += kLonePointHalfWidth;
            cover(left, right);
        }
        m_edges = 0;
        if (m_begin >= m_end)
        {
            return;
        }
        for (std::size_t column = m_begin; column < m_end; ++column)
        {
            ColumnSpan& span = m_spans[column];
            if (span.empty() && column > m_begin)
            {
                span = m_spans[column - 1];  // keeps the outline well formed
            }
            span.top    = std::clamp(span.top, m_clipTop, m_clipBottom);
            span.bottom = std::clamp(span.bottom, m_clipTop, m_clipBottom);
        }
        outlineColumns(m_grid, m_spans, m_begin, m_end, *m_out);
        std::fill(m_spans.begin() + static_cast<std::ptrdiff_t>(m_begin),
                  m_spans.begin() + static_cast<std::ptrdiff_t>(m_end), ColumnSpan{});
        m_begin = m_grid.count;
        m_end   = 0;
    }

private:
    // Adds the band between two edges to the columns it crosses: in each, what it covers at the
    // column's center, or at the nearest x it reaches.
    void cover(const BandEdge& a, const BandEdge& b)
    {
        const double gridRight = m_grid.edge(m_grid.count);
        if (m_grid.count == 0 || !(a.x <= b.x) || b.x < m_grid.left || a.x > gridRight)
        {
            return;
        }
        const std::size_t first = m_grid.columnAt(a.x);
        const std::size_t last  = m_grid.columnAt(b.x);
        for (std::size_t column = first; column <= last; ++column)
        {
            ColumnSpan& span = m_spans[column];
            if (a.x == b.x)
            {
                span.top    = std::min({span.top, a.top, b.top});
                span.bottom = std::max({span.bottom, a.bottom, b.bottom});
                continue;
            }
            const double share = (std::clamp(m_grid.center(column), a.x, b.x) - a.x) / (b.x - a.x);
            span.top           = std::min(span.top, a.top + (share * (b.top - a.top)));
            span.bottom        = std::max(span.bottom, a.bottom + (share * (b.bottom - a.bottom)));
        }
        m_begin = std::min(m_begin, first);
        m_end   = std::max(m_end, last + 1);
    }

    ColumnGrid              m_grid;
    double                  m_clipTop;
    double                  m_clipBottom;
    Polyline*               m_out;
    std::vector<ColumnSpan> m_spans;
    std::size_t             m_begin = m_grid.count;  // the columns of the current stretch
    std::size_t             m_end   = 0;
    BandEdge                m_last;
    std::size_t             m_edges = 0;  // in the current stretch
};

// What decimateErrorBand() reads, and the band it builds.
struct BandSource
{
    const SeriesData*       data;
    std::span<const double> low;
    std::span<const double> high;
    const AxisMapping*      x;
    const AxisMapping*      y;

    // The band at point index; a gap if the point or its bar can't be placed. (Whichever way the y
    // axis runs, the band's top is its end with the smaller pixel.)
    void addPoint(std::size_t index, BandBuilder& band) const
    {
        const double px     = x->toPixel(data->x(index));
        const double py     = y->toPixel(data->y(index));
        const double top    = highEndPixel(*y, high[index]);
        const double bottom = lowEndPixel(*y, low[index]);
        if (std::isfinite(px) && std::isfinite(py) && std::isfinite(top) && std::isfinite(bottom))
        {
            band.add(px, std::min(top, bottom), std::max(top, bottom));
        }
        else
        {
            band.gap();
        }
    }

    // The band over the points of one pixel column, [first, last): from its highest to its lowest
    // end, at the x of its first and of its last point. As for lines, gaps at the column's start
    // or end break the band; one inside it is narrower than a pixel and is drawn through.
    void addColumn(const ErrorData& errors, std::size_t first, std::size_t last,
                   BandBuilder& band) const
    {
        const MinMax highest = errors.yHighPyramid().query(high, first, last);
        const MinMax lowest  = errors.yLowPyramid().query(low, first, last);
        if (!highest.hasFinite() || !lowest.hasFinite())
        {
            band.gap();
            return;
        }
        const double top    = highEndPixel(*y, highest.max);
        const double bottom = lowEndPixel(*y, lowest.min);
        const double left   = x->toPixel(data->x(highest.firstFinite));
        const double right  = x->toPixel(data->x(highest.lastFinite));
        if (!std::isfinite(top) || !std::isfinite(bottom) || !std::isfinite(right))
        {
            band.gap();
            return;
        }
        if (highest.firstFinite != first || !std::isfinite(left))
        {
            band.gap();
        }
        if (std::isfinite(left) && left != right)
        {
            band.add(left, std::min(top, bottom), std::max(top, bottom));
        }
        band.add(right, std::min(top, bottom), std::max(top, bottom));
        if (highest.lastFinite != last - 1)
        {
            band.gap();
        }
    }
};

// A grid of cells over a box that each take one bar.
class CellGrid
{
public:
    CellGrid(PixelBox box, double cellSize) : m_box(box), m_cellSize(cellSize)
    {
        const double width  = box.right - box.left;
        const double height = box.bottom - box.top;
        if ((width / m_cellSize) * (height / m_cellSize) > kMaxBarCells)
        {
            m_cellSize = std::sqrt(width * height / kMaxBarCells);
        }
        m_columns = static_cast<std::size_t>(std::ceil(width / m_cellSize)) + 1;
        m_rows    = static_cast<std::size_t>(std::ceil(height / m_cellSize)) + 1;
        m_taken.assign(m_columns * m_rows, false);
    }

    // Whether the cell of @p point (the nearest one, for a point outside the box) was still free;
    // it is taken now.
    bool claim(PixelPoint point)
    {
        const std::size_t cell = (cellOf(point.y - m_box.top, m_rows) * m_columns) +
                                 cellOf(point.x - m_box.left, m_columns);
        if (m_taken[cell])
        {
            return false;
        }
        m_taken[cell] = true;
        return true;
    }

private:
    [[nodiscard]] std::size_t cellOf(double offset, std::size_t cells) const
    {
        return static_cast<std::size_t>(
            std::clamp(std::floor(offset / m_cellSize), 0.0, static_cast<double>(cells - 1)));
    }

    PixelBox          m_box;
    double            m_cellSize;
    std::size_t       m_columns = 0;
    std::size_t       m_rows    = 0;
    std::vector<bool> m_taken;
};

// What collectErrorBars() reads: the first xCount points have x errors, the first yCount y errors.
struct BarSource
{
    const SeriesData*  data;
    const ErrorData*   errors;
    const AxisMapping* x;
    const AxisMapping* y;
    std::size_t        xCount;
    std::size_t        yCount;

    // The whole error bar of point index; nothing for a point that can't be placed.
    [[nodiscard]] std::optional<ErrorBar> barAt(std::size_t index) const
    {
        const PixelPoint center{.x = x->toPixel(data->x(index)), .y = y->toPixel(data->y(index))};
        if (!std::isfinite(center.x) || !std::isfinite(center.y))
        {
            return std::nullopt;
        }
        ErrorBar bar{
            .center = center,
            .left   = center.x,
            .right  = center.x,
            .top    = center.y,
            .bottom = center.y,
        };
        if (index < xCount)
        {
            const double low  = lowEndPixel(*x, errors->xLow()[index]);
            const double high = highEndPixel(*x, errors->xHigh()[index]);
            bar.left          = std::min(low, high);
            bar.right         = std::max(low, high);
        }
        if (index < yCount)
        {
            const double low  = lowEndPixel(*y, errors->yLow()[index]);
            const double high = highEndPixel(*y, errors->yHigh()[index]);
            bar.top           = std::min(low, high);
            bar.bottom        = std::max(low, high);
        }
        return bar;
    }
};

// Cuts the whiskers of @p bar off at the edges of @p box, and drops one that doesn't cross the box
// (both its ends are then at the center). False if neither does.
bool cutToBox(ErrorBar& bar, PixelBox box)
{
    const PixelPoint center = bar.center;
    const bool across  = bar.left < bar.right && center.y >= box.top && center.y <= box.bottom &&
                         bar.right >= box.left && bar.left <= box.right;
    const bool upright = bar.top < bar.bottom && center.x >= box.left && center.x <= box.right &&
                         bar.bottom >= box.top && bar.top <= box.bottom;
    bar.left           = across ? std::max(bar.left, box.left) : center.x;
    bar.right          = across ? std::min(bar.right, box.right) : center.x;
    bar.top            = upright ? std::max(bar.top, box.top) : center.y;
    bar.bottom         = upright ? std::min(bar.bottom, box.bottom) : center.y;
    return across || upright;
}

}  // namespace

std::size_t collectErrorBars(const SeriesData& data, const ErrorData& errors, const AxisMapping& x,
                             const AxisMapping& y, PixelBox box, double cellSize, bool withY,
                             std::vector<ErrorBar>& out)
{
    out.clear();
    const BarSource source{
        .data   = &data,
        .errors = &errors,
        .x      = &x,
        .y      = &y,
        .xCount = std::min(data.size(), errors.xLow().size()),
        .yCount = withY ? std::min(data.size(), errors.yLow().size()) : 0,
    };
    const std::size_t count = std::max(source.xCount, source.yCount);
    if (count == 0 || !(box.right > box.left) || !(box.bottom > box.top) || !(cellSize > 0.0))
    {
        return 0;
    }
    std::size_t first = 0;
    std::size_t last  = count;
    if (data.isSortedByX())
    {
        // A point beside the box can reach into it by its x error.
        const double v0 = x.toValue(box.left);
        const double v1 = x.toValue(box.right);
        first           = std::min(count, data.lowerBound(std::min(v0, v1) - errors.xReachAbove()));
        last = std::clamp(data.upperBound(std::max(v0, v1) + errors.xReachBelow()), first, count);
    }
    CellGrid cells(box, cellSize);
    for (std::size_t i = first; i < last; ++i)
    {
        std::optional<ErrorBar> bar = source.barAt(i);
        if (bar && cutToBox(*bar, box) && cells.claim(bar->center))
        {
            out.push_back(*bar);
        }
    }
    return last - first;
}

std::size_t decimateErrorBand(const SeriesData& data, const ErrorData& errors, const AxisMapping& x,
                              const AxisMapping& y, const ColumnGrid& grid, double clipTop,
                              double clipBottom, Polyline& out)
{
    out.clear();
    const std::size_t count = std::min(data.size(), errors.yLow().size());
    if (count == 0 || !data.isSortedByX() || grid.count == 0 || !(grid.width > 0.0) ||
        !(clipTop <= clipBottom))
    {
        return 0;
    }
    auto [start, stop] = visibleIndexRange(data, x.range());
    stop               = std::min(stop, count);
    if (start >= stop)
    {
        return 0;
    }
    const BandSource source{
        .data = &data,
        .low  = errors.yLow(),
        .high = errors.yHigh(),
        .x    = &x,
        .y    = &y,
    };
    BandBuilder       band(grid, clipTop, clipBottom, out);
    const std::size_t visibleCount = stop - start;
    const double      columns      = std::max(1.0, std::ceil(x.pixelLength() / grid.width));
    if (static_cast<double>(visibleCount) <= static_cast<double>(kRawPointsPerColumn) * columns)
    {
        for (std::size_t i = start; i < stop; ++i)
        {
            source.addPoint(i, band);
        }
        band.gap();
        return visibleCount;
    }

    // One pixel column at a time, as decimateLine() does.
    const auto   columnCount = static_cast<std::size_t>(columns);
    const double direction   = directionOf(x);
    std::size_t  a           = start;
    for (std::size_t column = 0; column <= columnCount && a < stop; ++column)
    {
        std::size_t b = stop;
        if (column < columnCount)
        {
            const double edge = x.toValue(
                x.pixelStart() + (direction * grid.width * static_cast<double>(column + 1)));
            b = std::clamp(data.lowerBound(edge), a, stop);
        }
        if (b - a > kRawPointsPerColumn)
        {
            source.addColumn(errors, a, b, band);
        }
        else
        {
            for (std::size_t i = a; i < b; ++i)
            {
                source.addPoint(i, band);
            }
        }
        a = b;
    }
    band.gap();
    return visibleCount;
}

}  // namespace rocketplot::core
