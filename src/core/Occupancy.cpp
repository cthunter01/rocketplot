#include "core/Occupancy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/Decimator.h"

namespace rocketplot::core
{

namespace
{

// Pixels per cell: legend rows are about 20 pixels tall.
constexpr double kCell = 4.0;

// The vertical span a line covers in one cell column, built up segment by segment.
struct Span
{
    std::size_t column = 0;
    double      top    = 0.0;
    double      bottom = 0.0;
};

}  // namespace

void Occupancy::reset(PixelBox area)
{
    m_area = area;
    m_columns =
        static_cast<std::size_t>(std::max(1.0, std::ceil((area.right - area.left) / kCell)));
    m_rows = static_cast<std::size_t>(std::max(1.0, std::ceil((area.bottom - area.top) / kCell)));
    m_cells.assign(m_columns * m_rows, 0);
    m_summed = false;
}

void Occupancy::clear()
{
    m_columns = 0;
    m_rows    = 0;
    m_cells.clear();
    m_summed = false;
}

std::size_t Occupancy::columnOf(double x) const
{
    const double cell = std::floor((x - m_area.left) / kCell);
    return static_cast<std::size_t>(std::clamp(cell, 0.0, static_cast<double>(m_columns - 1)));
}

std::size_t Occupancy::rowOf(double y) const
{
    const double cell = std::floor((y - m_area.top) / kCell);
    return static_cast<std::size_t>(std::clamp(cell, 0.0, static_cast<double>(m_rows - 1)));
}

void Occupancy::markColumn(std::size_t column, double top, double bottom)
{
    if (bottom < m_area.top || top > m_area.bottom)
    {
        return;
    }
    const std::size_t last = rowOf(bottom);
    for (std::size_t row = rowOf(top); row <= last; ++row)
    {
        m_cells[(row * m_columns) + column] = 1;
    }
}

void Occupancy::addPolyline(const Polyline& line, double width)
{
    if (!isRecorded())
    {
        return;
    }
    m_summed                 = false;
    const double        half = width / 2.0;
    std::optional<Span> span;
    // Dense data puts many segments in one cell column: merge them, then mark the column once.
    const auto extend = [&](std::size_t column, double y0, double y1) {
        const double top    = std::min(y0, y1) - half;
        const double bottom = std::max(y0, y1) + half;
        if (span && span->column == column)
        {
            span->top    = std::min(span->top, top);
            span->bottom = std::max(span->bottom, bottom);
            return;
        }
        if (span)
        {
            markColumn(span->column, span->top, span->bottom);
        }
        span = Span{.column = column, .top = top, .bottom = bottom};
    };
    for (std::size_t r = 0; r < line.runCount(); ++r)
    {
        const auto run = line.run(r);
        if (run.size() == 1)
        {
            const PixelPoint point = run.front();
            if (point.x >= m_area.left && point.x <= m_area.right)
            {
                extend(columnOf(point.x), point.y, point.y);
            }
            continue;
        }
        for (std::size_t i = 0; i + 1 < run.size(); ++i)
        {
            const PixelPoint a     = run[i];
            const PixelPoint b     = run[i + 1];
            const double     left  = std::max(std::min(a.x, b.x), m_area.left);
            const double     right = std::min(std::max(a.x, b.x), m_area.right);
            if (left > right)
            {
                continue;  // beside the area
            }
            if (a.x == b.x)
            {
                extend(columnOf(a.x), a.y, b.y);
                continue;
            }
            // The segment's y where it enters and leaves each cell column it crosses.
            const auto yAt = [&](double x) {
                return a.y + ((b.y - a.y) * (x - a.x) / (b.x - a.x));
            };
            const std::size_t last = columnOf(right);
            for (std::size_t column = columnOf(left); column <= last; ++column)
            {
                const double columnLeft = m_area.left + (static_cast<double>(column) * kCell);
                extend(column, yAt(std::max(left, columnLeft)),
                       yAt(std::min(right, columnLeft + kCell)));
            }
        }
    }
    if (span)
    {
        markColumn(span->column, span->top, span->bottom);
    }
}

void Occupancy::addPoints(const std::vector<PixelPoint>& points, double size)
{
    if (!isRecorded())
    {
        return;
    }
    m_summed          = false;
    const double half = size / 2.0;
    for (const PixelPoint& point : points)
    {
        if (point.x + half < m_area.left || point.x - half > m_area.right)
        {
            continue;
        }
        const std::size_t last = columnOf(point.x + half);
        for (std::size_t column = columnOf(point.x - half); column <= last; ++column)
        {
            markColumn(column, point.y - half, point.y + half);
        }
    }
}

void Occupancy::sum() const
{
    const std::size_t stride = m_columns + 1;
    m_sums.assign(stride * (m_rows + 1), 0);
    for (std::size_t row = 0; row < m_rows; ++row)
    {
        std::uint32_t inRow = 0;
        for (std::size_t column = 0; column < m_columns; ++column)
        {
            inRow += m_cells[(row * m_columns) + column] != 0 ? 1U : 0U;
            m_sums[((row + 1) * stride) + column + 1] = m_sums[(row * stride) + column + 1] + inRow;
        }
    }
    m_summed = true;
}

double Occupancy::coverage(PixelBox box) const
{
    if (!isRecorded())
    {
        return 0.0;
    }
    if (!m_summed)
    {
        sum();
    }
    // The cells the box covers at least half of.
    const auto cell = [](double offset, std::size_t count) {
        return static_cast<std::size_t>(
            std::clamp(std::round(offset / kCell), 0.0, static_cast<double>(count)));
    };
    const std::size_t left   = cell(box.left - m_area.left, m_columns);
    const std::size_t right  = cell(box.right - m_area.left, m_columns);
    const std::size_t top    = cell(box.top - m_area.top, m_rows);
    const std::size_t bottom = cell(box.bottom - m_area.top, m_rows);
    if (right <= left || bottom <= top)
    {
        return 0.0;
    }
    const std::size_t   stride = m_columns + 1;
    const std::uint32_t count = m_sums[(bottom * stride) + right] - m_sums[(top * stride) + right] -
                                m_sums[(bottom * stride) + left] + m_sums[(top * stride) + left];
    return static_cast<double>(count) / static_cast<double>((right - left) * (bottom - top));
}

}  // namespace rocketplot::core
