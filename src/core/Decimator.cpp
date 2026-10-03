#include "core/Decimator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/AxisMapping.h"
#include "core/MinMaxPyramid.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// The scatter occupancy grid is capped at this many cells (8 MB); larger boxes use coarser cells.
constexpr double kMaxScatterCells = 64.0 * 1024.0 * 1024.0;

// Appends data points to a Polyline as pixels, ending the run wherever a gap was seen since the
// last point.
class LineBuilder
{
public:
    LineBuilder(const SeriesData& data, const AxisMapping& x, const AxisMapping& y, Polyline& out)
      : m_data(&data), m_x(&x), m_y(&y), m_out(&out)
    {
    }

    void gap() noexcept { m_gap = true; }

    // The point at index; a gap if it has no finite pixel position (NaN or infinite data, or a
    // value <= 0 on a log axis).
    void addIndex(std::size_t index)
    {
        const PixelPoint point{
            .x = m_x->toPixel(m_data->x(index)),
            .y = m_y->toPixel(m_data->y(index)),
        };
        if (std::isfinite(point.x) && std::isfinite(point.y))
        {
            addPixel(point);
        }
        else
        {
            gap();
        }
    }

    // The points in [first, last).
    void addRaw(std::size_t first, std::size_t last)
    {
        for (std::size_t i = first; i < last; ++i)
        {
            addIndex(i);
        }
    }

    void addPixel(PixelPoint point)
    {
        if (m_gap)
        {
            m_out->endRun();
            m_gap = false;
        }
        m_out->add(point);
    }

    void finish() { m_out->endRun(); }

private:
    const SeriesData*  m_data;
    const AxisMapping* m_x;
    const AxisMapping* m_y;
    Polyline*          m_out;
    bool               m_gap = false;
};

// The points of one pixel column, [first, last) with more than kRawPointsPerColumn points, as its
// first, lowest, highest and last finite point in index order. Gaps at the column's start or end
// break the line; a gap inside it is narrower than a pixel and is drawn through.
void addColumn(const SeriesData& data, std::size_t first, std::size_t last, LineBuilder& line)
{
    const MinMax summary = data.pyramid().query(data.ys(), first, last);
    if (!summary.hasFinite())
    {
        line.gap();
        return;
    }
    if (summary.firstFinite != first)
    {
        line.gap();
    }
    std::array<std::size_t, 4> indices{
        summary.firstFinite,
        summary.argMin,
        summary.argMax,
        summary.lastFinite,
    };
    std::ranges::sort(indices);
    const auto duplicates = std::ranges::unique(indices);
    for (const std::size_t index : std::ranges::subrange(indices.begin(), duplicates.begin()))
    {
        line.addIndex(index);
    }
    if (summary.lastFinite != last - 1)
    {
        line.gap();
    }
}

// Unsorted x: every finite point, except one that falls in the same pixel cell as the point before
// it (the segment between them stays inside that cell, so it draws nothing new).
DecimationResult decimateUnsorted(const SeriesData& data, const AxisMapping& x,
                                  const AxisMapping& y, double columnWidth, LineBuilder& line)
{
    bool   havePrevious = false;
    double previousCol  = 0.0;
    double previousRow  = 0.0;
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        const PixelPoint p{.x = x.toPixel(data.x(i)), .y = y.toPixel(data.y(i))};
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
        {
            line.gap();  // NaN or infinite data, or a value <= 0 on a log axis
            havePrevious = false;
            continue;
        }
        const double col = std::floor(p.x / columnWidth);
        const double row = std::floor(p.y / columnWidth);
        if (havePrevious && col == previousCol && row == previousRow)
        {
            continue;
        }
        line.addPixel(p);
        previousCol  = col;
        previousRow  = row;
        havePrevious = true;
    }
    return {.mode = DecimationMode::PIXEL_SKIP, .visiblePoints = data.size()};
}

}  // namespace

void Polyline::clear() noexcept
{
    m_points.clear();
    m_runEnds.clear();
}

void Polyline::endRun()
{
    const std::size_t runStart = m_runEnds.empty() ? 0 : m_runEnds.back();
    if (m_points.size() > runStart)
    {
        m_runEnds.push_back(m_points.size());
    }
}

std::size_t Polyline::runCount() const noexcept
{
    const std::size_t runStart = m_runEnds.empty() ? 0 : m_runEnds.back();
    return m_runEnds.size() + (m_points.size() > runStart ? 1 : 0);
}

std::span<const PixelPoint> Polyline::run(std::size_t index) const noexcept
{
    const std::size_t begin = index == 0 ? 0 : m_runEnds[index - 1];
    const std::size_t end   = index < m_runEnds.size() ? m_runEnds[index] : m_points.size();
    return std::span<const PixelPoint>(m_points).subspan(begin, end - begin);
}

std::pair<std::size_t, std::size_t> visibleIndexRange(const SeriesData& data, Range visible)
{
    const std::size_t n     = data.size();
    std::size_t       start = data.lowerBound(visible.min);
    std::size_t       stop  = data.upperBound(visible.max);
    start                   = start > 0 ? start - 1 : 0;
    stop                    = stop < n ? stop + 1 : n;
    while (start < stop && !std::isfinite(data.x(start)))
    {
        ++start;
    }
    while (stop > start && !std::isfinite(data.x(stop - 1)))
    {
        --stop;
    }
    return {start, stop};
}

DecimationResult decimateLine(const SeriesData& data, const AxisMapping& x, const AxisMapping& y,
                              double columnWidth, Polyline& out)
{
    out.clear();
    if (data.empty() || !(columnWidth > 0.0))
    {
        return {};
    }
    LineBuilder line(data, x, y, out);
    if (!data.isSortedByX())
    {
        const DecimationResult result = decimateUnsorted(data, x, y, columnWidth, line);
        line.finish();
        return result;
    }

    const Range visible            = x.range();
    const auto [start, stop]       = visibleIndexRange(data, visible);
    const std::size_t visibleCount = stop - start;
    const double      columns      = std::max(1.0, std::ceil(x.pixelLength() / columnWidth));
    if (static_cast<double>(visibleCount) <= static_cast<double>(kRawPointsPerColumn) * columns)
    {
        line.addRaw(start, stop);
        line.finish();
        return {.mode = DecimationMode::RAW, .visiblePoints = visibleCount};
    }

    // One pixel column at a time: its index range [a, b) by binary search for the data value at its
    // right edge (through the mapping, so log axes work), its extremes from the pyramid. The last
    // iteration takes everything right of the visible range.
    const auto   columnCount = static_cast<std::size_t>(columns);
    const double direction   = x.pixelEnd() >= x.pixelStart() ? 1.0 : -1.0;
    std::size_t  a           = start;
    for (std::size_t column = 0; column <= columnCount && a < stop; ++column)
    {
        std::size_t b = stop;
        if (column < columnCount)
        {
            const double edge = x.toValue(
                x.pixelStart() + (direction * columnWidth * static_cast<double>(column + 1)));
            b = std::clamp(data.lowerBound(edge), a, stop);
        }
        // An empty column needs nothing: the line just crosses it.
        if (b - a > kRawPointsPerColumn)
        {
            addColumn(data, a, b, line);
        }
        else
        {
            line.addRaw(a, b);
        }
        a = b;
    }
    line.finish();
    return {.mode = DecimationMode::MIN_MAX, .visiblePoints = visibleCount};
}

std::size_t decimateScatter(const SeriesData& data, const AxisMapping& x, const AxisMapping& y,
                            PixelBox box, double cellSize, std::vector<PixelPoint>& out,
                            std::vector<std::size_t>* indices, std::span<const double> sizes)
{
    out.clear();
    if (indices != nullptr)
    {
        indices->clear();
    }
    const double width  = box.right - box.left;
    const double height = box.bottom - box.top;
    if (data.empty() || !(width > 0.0) || !(height > 0.0) || !(cellSize > 0.0))
    {
        return 0;
    }
    std::size_t first = 0;
    std::size_t last  = data.size();
    if (data.isSortedByX())
    {
        const double v0 = x.toValue(box.left);
        const double v1 = x.toValue(box.right);
        first           = data.lowerBound(std::min(v0, v1));
        last            = std::max(first, data.upperBound(std::max(v0, v1)));
    }

    if ((width / cellSize) * (height / cellSize) > kMaxScatterCells)
    {
        cellSize = std::sqrt(width * height / kMaxScatterCells);
    }
    const auto        cols = static_cast<std::size_t>(std::ceil(width / cellSize)) + 1;
    const auto        rows = static_cast<std::size_t>(std::ceil(height / cellSize)) + 1;
    std::vector<bool> occupied(cols * rows, false);

    for (std::size_t i = first; i < last; ++i)
    {
        const PixelPoint p{.x = x.toPixel(data.x(i)), .y = y.toPixel(data.y(i))};
        if (!box.contains(p))  // also false for a NaN or infinite position
        {
            continue;
        }
        if (i < sizes.size() && !(sizes[i] > 0.0))
        {
            continue;  // a hidden marker doesn't take the cell from one that shows
        }
        const auto        col  = static_cast<std::size_t>((p.x - box.left) / cellSize);
        const auto        row  = static_cast<std::size_t>((p.y - box.top) / cellSize);
        const std::size_t cell = (row * cols) + col;
        if (occupied[cell])
        {
            continue;
        }
        occupied[cell] = true;
        out.push_back(p);
        if (indices != nullptr)
        {
            indices->push_back(i);
        }
    }
    return last - first;
}

}  // namespace rocketplot::core
