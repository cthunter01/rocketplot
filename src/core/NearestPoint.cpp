#include "core/NearestPoint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/MinMaxPyramid.h"
#include "core/SeriesData.h"

namespace rocketplot::core
{

namespace
{

// A run of this many points or fewer is read point by point: asking the pyramid about it first
// would cost about as much.
constexpr std::size_t kLeafSize = 256;
// The width in pixels of the column whose points are all at a traced position's x.
constexpr double kTraceColumn = 1.0;
// The x values at the ends of a pixel interval are worked out with rounding errors: the points
// are looked for this much farther out, and then tested against the pixels themselves.
constexpr double kSlack = 1e-6;

// How far @p value is outside the interval between @p a and @p b: 0 inside it, and when it isn't
// known where the interval is (an end that is NaN).
double distanceOutside(double value, double a, double b) noexcept
{
    if (std::isnan(a) || std::isnan(b))
    {
        return 0.0;
    }
    const double low  = std::min(a, b);
    const double high = std::max(a, b);
    if (value < low)
    {
        return low - value;
    }
    return value > high ? value - high : 0.0;
}

// Looks through the points of a series for the one drawn nearest a position.
class Search
{
public:
    Search(const SeriesData& data, const AxisMapping& x, const AxisMapping& y, PixelBox box,
           PixelPoint position, double reach, std::span<const double> sizes)
      : m_data(&data),
        m_x(&x),
        m_y(&y),
        m_box(box),
        m_position(position),
        m_sizes(sizes),
        m_nearest(reach * reach)
    {
    }

    // The point at @p index, if it is nearer than the nearest so far (or as near, and before it).
    void consider(std::size_t index)
    {
        if (index < m_sizes.size() && !(m_sizes[index] > 0.0))
        {
            return;  // a hidden marker
        }
        const PixelPoint pixel{
            .x = m_x->toPixel(m_data->x(index)),
            .y = m_y->toPixel(m_data->y(index)),
        };
        if (!m_box.contains(pixel))  // also false for a gap: a NaN or infinite position
        {
            return;
        }
        const double dx      = pixel.x - m_position.x;
        const double dy      = pixel.y - m_position.y;
        const double squared = (dx * dx) + (dy * dy);
        if (squared < m_nearest || (squared == m_nearest && (!m_found || index < m_index)))
        {
            m_nearest = squared;
            m_index   = index;
            m_pixel   = pixel;
            m_found   = true;
        }
    }

    // The points in [first, last), one by one.
    void scan(std::size_t first, std::size_t last)
    {
        for (std::size_t i = first; i < last; ++i)
        {
            consider(i);
        }
    }

    // The points of a series sorted by x that are drawn between the pixels @p left and @p right.
    void searchBetween(double left, double right)
    {
        left  = std::max(left, m_box.left) - kSlack;
        right = std::min(right, m_box.right) + kSlack;
        if (!(left <= right))
        {
            return;
        }
        const double      a     = m_x->toValue(left);
        const double      b     = m_x->toValue(right);
        const std::size_t first = m_data->lowerBound(std::min(a, b));
        const std::size_t last  = m_data->upperBound(std::max(a, b));
        if (first < last)
        {
            searchSorted(first, last);
        }
    }

    [[nodiscard]] std::optional<NearestPoint> result() const
    {
        if (!m_found)
        {
            return std::nullopt;
        }
        return NearestPoint{.index = m_index, .pixel = m_pixel, .distance = std::sqrt(m_nearest)};
    }

private:
    // The points in [first, last) of a series sorted by x. They are drawn inside a box (from the
    // first's x to the last's, from their lowest y to their highest): when all of it is farther
    // away than the nearest point so far, none of them is looked at. Otherwise their extremes are
    // tried (the nearest there is to a position above or below them all) and each half is
    // searched in turn, the nearer first, so that the other one is likely to be ruled out.
    void searchSorted(std::size_t first, std::size_t last)
    {
        if (last - first <= kLeafSize)
        {
            scan(first, last);
            return;
        }
        const MinMax summary = m_data->pyramid().query(m_data->ys(), first, last);
        if (!summary.hasFinite())
        {
            return;
        }
        double lowest = summary.min;
        if (m_y->scale() == Scale::LOG)
        {
            if (!(summary.max > 0.0))
            {
                return;  // nothing a log axis shows
            }
            lowest = summary.minPositive;
        }
        const double left  = m_x->toPixel(m_data->x(first));
        const double right = m_x->toPixel(m_data->x(last - 1));
        const double dx    = distanceOutside(m_position.x, left, right);
        const double dy =
            distanceOutside(m_position.y, m_y->toPixel(lowest), m_y->toPixel(summary.max));
        if ((dx * dx) + (dy * dy) > m_nearest)
        {
            return;
        }
        consider(summary.argMin);
        consider(summary.argMax);
        const std::size_t middle = first + ((last - first) / 2);
        const double      split  = m_x->toPixel(m_data->x(middle));
        if (distanceOutside(m_position.x, left, split) <=
            distanceOutside(m_position.x, split, right))
        {
            searchSorted(first, middle);
            searchSorted(middle, last);
        }
        else
        {
            searchSorted(middle, last);
            searchSorted(first, middle);
        }
    }

    const SeriesData*       m_data;
    const AxisMapping*      m_x;
    const AxisMapping*      m_y;
    PixelBox                m_box;
    PixelPoint              m_position;
    std::span<const double> m_sizes;
    double                  m_nearest;  // the square of the distance a point has to beat
    std::size_t             m_index = 0;
    PixelPoint              m_pixel;
    bool                    m_found = false;
};

}  // namespace

std::optional<NearestPoint> nearestPoint(const SeriesData& data, const AxisMapping& x,
                                         const AxisMapping& y, PixelBox box, PixelPoint position,
                                         double reach, std::span<const double> sizes)
{
    if (!(reach >= 0.0))
    {
        return std::nullopt;
    }
    Search search(data, x, y, box, position, reach, sizes);
    if (data.isSortedByX())
    {
        search.searchBetween(position.x - reach, position.x + reach);
    }
    else
    {
        search.scan(0, data.size());
    }
    return search.result();
}

std::optional<NearestPoint> tracedPoint(const SeriesData& data, const AxisMapping& x,
                                        const AxisMapping& y, PixelBox box, PixelPoint position,
                                        std::span<const double> sizes)
{
    constexpr double kUnlimited = std::numeric_limits<double>::infinity();
    if (!data.isSortedByX())
    {
        return nearestPoint(data, x, y, box, position, kUnlimited, sizes);
    }
    Search search(data, x, y, box, position, kUnlimited, sizes);
    search.searchBetween(position.x - (kTraceColumn / 2.0), position.x + (kTraceColumn / 2.0));
    if (const std::optional<NearestPoint> inColumn = search.result())
    {
        return inColumn;
    }
    // The points are farther apart than a pixel here: the one before the position or the one
    // after it, whichever is nearer in x and can be shown.
    const std::size_t after = data.lowerBound(x.toValue(position.x));
    if (after == 0 || after >= data.size())
    {
        return std::nullopt;
    }
    const std::size_t before      = after - 1;
    const bool        beforeFirst = std::abs(position.x - x.toPixel(data.x(before))) <=
                                    std::abs(x.toPixel(data.x(after)) - position.x);
    for (const std::size_t index :
         beforeFirst ? std::array{before, after} : std::array{after, before})
    {
        search.consider(index);
        if (const std::optional<NearestPoint> neighbor = search.result())
        {
            return neighbor;
        }
    }
    return std::nullopt;
}

}  // namespace rocketplot::core
