#include "core/ErrorData.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>

#include "core/MinMaxPyramid.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// An error as a distance: its sign doesn't matter, and one that isn't a number is no error.
double magnitude(double error)
{
    return std::isfinite(error) ? std::abs(error) : 0.0;
}

}  // namespace

ErrorData::Ends ErrorData::endsOf(const SeriesData& data, bool alongX,
                                  std::span<const double> minus, std::span<const double> plus)
{
    const std::size_t count = data.size();
    if (minus.size() != count || plus.size() != count)
    {
        throw std::invalid_argument(std::format(
            "rocketplot: errors must have one value per point (the series has {} points, the "
            "errors {} and {})",
            count, minus.size(), plus.size()));
    }
    Ends ends;
    ends.low.resize(count);
    ends.high.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double x = data.x(i);
        const double y = data.y(i);
        if (!std::isfinite(x) || !std::isfinite(y))
        {
            ends.low[i]  = std::numeric_limits<double>::quiet_NaN();
            ends.high[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        const double value = alongX ? x : y;
        const double below = magnitude(minus[i]);
        const double above = magnitude(plus[i]);
        const double low   = value - below;
        const double high  = value + above;
        ends.low[i]        = low;
        ends.high[i]       = high;
        ends.bounds        = ends.bounds.including(low).including(high);
        ends.reachBelow    = std::max(ends.reachBelow, below);
        ends.reachAbove    = std::max(ends.reachAbove, above);
        if (value > 0.0)
        {
            ends.positive = ends.positive.including(high);
            if (low > 0.0)
            {
                ends.positive = ends.positive.including(low);
            }
        }
    }
    return ends;
}

void ErrorData::setX(const SeriesData& data, std::span<const double> minus,
                     std::span<const double> plus)
{
    m_x = endsOf(data, true, minus, plus);
}

void ErrorData::setY(const SeriesData& data, std::span<const double> minus,
                     std::span<const double> plus)
{
    m_y = endsOf(data, false, minus, plus);
    m_yLowPyramid.build(m_y.low);
    m_yHighPyramid.build(m_y.high);
}

void ErrorData::clear()
{
    m_x = {};
    m_y = {};
    m_yLowPyramid.clear();
    m_yHighPyramid.clear();
}

Range ErrorData::yBoundsWithin(const SeriesData& data, Range xRange, bool positiveOnly) const
{
    const std::size_t count = std::min(data.size(), m_y.low.size());
    if (count == 0 || !(xRange.min <= xRange.max))
    {
        return Range::empty();
    }
    if (!data.isSortedByX())
    {
        Range bounds = Range::empty();
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!xRange.contains(data.x(i)))
            {
                continue;
            }
            for (const double end : {m_y.low[i], m_y.high[i]})
            {
                if (!positiveOnly || end > 0.0)
                {
                    bounds = bounds.including(end);
                }
            }
        }
        return bounds;
    }
    const std::size_t first = data.lowerBound(xRange.min);
    const std::size_t last  = std::min(data.upperBound(xRange.max), count);
    if (first >= last)
    {
        return Range::empty();
    }
    const MinMax low  = m_yLowPyramid.query(m_y.low, first, last);
    const MinMax high = m_yHighPyramid.query(m_y.high, first, last);
    if (!low.hasFinite() || !high.hasFinite())
    {
        return Range::empty();
    }
    if (!positiveOnly)
    {
        return {.min = low.min, .max = high.max};
    }
    if (!(high.max > 0.0))
    {
        return Range::empty();
    }
    // The lowest end a log axis can show: a low end if any is positive, else a high one.
    return {.min = std::min(low.minPositive, high.minPositive), .max = high.max};
}

}  // namespace rocketplot::core
