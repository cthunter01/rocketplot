#include "core/LineBand.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>

#include "core/Decimator.h"

namespace rocketplot::core
{

namespace
{

// Columns per polygon: QPainter fills an antialiased polygon in time that grows with its bounding
// box, so a band is filled in narrow pieces (16 columns: about 4x faster than one polygon for a
// 1500-column band).
constexpr std::size_t kColumnsPerPolygon = 16;
// Segments narrower than this (in pixels) are treated as vertical.
constexpr double kVerticalWidth = 1e-9;

// The area a round pen of radius r covers along segment a-b (a.x <= b.x): a "capsule". At a given
// x, it covers one vertical interval; these give its ends. Both assume x is within [a.x - r, b.x +
// r].
class Capsule
{
public:
    Capsule(PixelPoint a, PixelPoint b, double radius)
      : m_a(a), m_b(b), m_radius(radius), m_vertical(b.x - a.x < kVerticalWidth)
    {
        if (!m_vertical)
        {
            m_slope = (b.y - a.y) / (b.x - a.x);
            // The segment point whose circle reaches furthest down (or up) at x lies this far right
            // (left) of x.
            m_offset = radius * m_slope / std::sqrt(1.0 + (m_slope * m_slope));
        }
    }

    // The largest y covered at x.
    [[nodiscard]] double maxYAt(double x) const { return reach(x, m_offset, 1.0); }
    // The smallest y covered at x.
    [[nodiscard]] double minYAt(double x) const { return reach(x, -m_offset, -1.0); }

private:
    // y of the segment point at offset from x (clamped to the segment and the circle), plus or
    // minus the circle.
    [[nodiscard]] double reach(double x, double offset, double sign) const
    {
        if (m_vertical)
        {
            const double dx = x - m_a.x;
            const double y  = sign > 0.0 ? std::max(m_a.y, m_b.y) : std::min(m_a.y, m_b.y);
            return y + (sign * std::sqrt(std::max(0.0, (m_radius * m_radius) - (dx * dx))));
        }
        const double low  = std::max(m_a.x, x - m_radius);
        const double high = std::min(m_b.x, x + m_radius);
        const double at =
            low <= high ? std::clamp(x + offset, low, high) : std::clamp(x, m_a.x, m_b.x);
        const double y  = m_a.y + (m_slope * (at - m_a.x));
        const double dx = x - at;
        return y + (sign * std::sqrt(std::max(0.0, (m_radius * m_radius) - (dx * dx))));
    }

    PixelPoint m_a;
    PixelPoint m_b;
    double     m_radius;
    bool       m_vertical;
    double     m_slope  = 0.0;
    double     m_offset = 0.0;
};

}  // namespace

std::size_t ColumnGrid::columnAt(double x) const noexcept
{
    const double column = std::floor((x - left) / width);
    if (!(column > 0.0))
    {
        return 0;
    }
    return std::min(count - 1,
                    static_cast<std::size_t>(std::min(column, static_cast<double>(count))));
}

void LineBand::cover(PixelPoint a, PixelPoint b, const ColumnGrid& grid, double halfWidth)
{
    if (a.x > b.x)
    {
        std::swap(a, b);
    }
    const Capsule capsule(a, b, halfWidth);
    // Lines at least a column wide are sampled at column centers, so they get their true width in
    // columns; thinner ones over (nearly) the whole column, so they never vanish between centers.
    const double margin = std::min(halfWidth, grid.width / 2.0);
    // Where the capsule reaches lowest and highest: under the endpoint with the larger (smaller) y.
    const double      xOfMaxY = a.y >= b.y ? a.x : b.x;
    const double      xOfMinY = a.y >= b.y ? b.x : a.x;
    const std::size_t first   = grid.columnAt(a.x - halfWidth);
    const std::size_t last    = grid.columnAt(b.x + halfWidth);
    for (std::size_t column = first; column <= last; ++column)
    {
        const double columnLeft = grid.left + (static_cast<double>(column) * grid.width);
        const double from       = std::max(columnLeft + margin, a.x - halfWidth);
        const double to         = std::min(columnLeft + grid.width - margin, b.x + halfWidth);
        if (from > to)
        {
            continue;
        }
        // The capsule's lower edge is concave and its upper edge convex, so over [from, to] their
        // extremes are where the overall extremes are, clamped into the interval.
        ColumnSpan& span = m_spans[column];
        span.top         = std::min(span.top, capsule.minYAt(std::clamp(xOfMinY, from, to)));
        span.bottom      = std::max(span.bottom, capsule.maxYAt(std::clamp(xOfMaxY, from, to)));
    }
}

void LineBand::outline(const Polyline& line, const ColumnGrid& grid, double halfWidth,
                       Polyline& polygons)
{
    polygons.clear();
    if (grid.count == 0 || !(grid.width > 0.0) || !(halfWidth > 0.0))
    {
        return;
    }
    m_spans.assign(grid.count, ColumnSpan{});
    for (std::size_t r = 0; r < line.runCount(); ++r)
    {
        const auto run = line.run(r);
        if (run.empty())
        {
            continue;
        }
        const auto [left, right] = std::ranges::minmax(run, {}, &PixelPoint::x);
        const std::size_t first  = grid.columnAt(left.x - halfWidth);
        const std::size_t last   = grid.columnAt(right.x + halfWidth);
        std::fill(m_spans.begin() + static_cast<std::ptrdiff_t>(first),
                  m_spans.begin() + static_cast<std::ptrdiff_t>(last) + 1, ColumnSpan{});
        cover(run.front(), run.front(), grid, halfWidth);  // a lone point is a dot
        for (std::size_t i = 1; i < run.size(); ++i)
        {
            cover(run[i - 1], run[i], grid, halfWidth);
        }

        std::size_t begin = first;
        std::size_t end   = last + 1;
        while (begin < end && m_spans[begin].empty())
        {
            ++begin;
        }
        while (end > begin && m_spans[end - 1].empty())
        {
            --end;
        }
        if (begin == end)
        {
            continue;
        }
        // A connected run covers every column between its ends; keep the outline well formed
        // regardless.
        for (std::size_t column = begin + 1; column < end; ++column)
        {
            if (m_spans[column].empty())
            {
                m_spans[column] = m_spans[column - 1];
            }
        }
        outlineColumns(grid, m_spans, begin, end, polygons);
    }
}

void outlineColumns(const ColumnGrid& grid, std::span<const ColumnSpan> spans, std::size_t begin,
                    std::size_t end, Polyline& polygons)
{
    const auto halfway = [&](std::size_t column) {
        const ColumnSpan& a = spans[column - 1];
        const ColumnSpan& b = spans[column];
        return ColumnSpan{.top = (a.top + b.top) / 2.0, .bottom = (a.bottom + b.bottom) / 2.0};
    };
    // Each piece: along the tops through the column centers, then back along the bottoms. The outer
    // columns reach their outer edges; where two pieces meet, both use the outline's value halfway
    // between the column centers, so they join without a seam.
    for (std::size_t pieceBegin = begin; pieceBegin < end; pieceBegin += kColumnsPerPolygon)
    {
        const std::size_t pieceEnd = std::min(end, pieceBegin + kColumnsPerPolygon);
        const ColumnSpan  start    = pieceBegin == begin ? spans[begin] : halfway(pieceBegin);
        const ColumnSpan  finish   = pieceEnd == end ? spans[end - 1] : halfway(pieceEnd);
        polygons.add({.x = grid.edge(pieceBegin), .y = start.top});
        for (std::size_t column = pieceBegin; column < pieceEnd; ++column)
        {
            polygons.add({.x = grid.center(column), .y = spans[column].top});
        }
        polygons.add({.x = grid.edge(pieceEnd), .y = finish.top});
        polygons.add({.x = grid.edge(pieceEnd), .y = finish.bottom});
        for (std::size_t column = pieceEnd; column-- > pieceBegin;)
        {
            polygons.add({.x = grid.center(column), .y = spans[column].bottom});
        }
        polygons.add({.x = grid.edge(pieceBegin), .y = start.bottom});
        polygons.endRun();
    }
}

}  // namespace rocketplot::core
