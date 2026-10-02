#include "core/SeriesData.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/MinMaxPyramid.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace rocketplot::core
{

namespace
{

void requireSameSize(std::size_t xSize, std::size_t ySize)
{
    if (xSize != ySize)
    {
        throw std::invalid_argument(std::format(
            "rocketplot: x and y must have the same size (x has {}, y has {})", xSize, ySize));
    }
}

bool isSortedUniform(UniformX x)
{
    return std::isfinite(x.start) && std::isfinite(x.step) && x.step >= 0.0;
}

}  // namespace

void SeriesData::setOwned(std::vector<double> x, std::vector<double> y)
{
    requireSameSize(x.size(), y.size());
    m_xOwned    = std::move(x);
    m_yOwned    = std::move(y);
    m_x         = m_xOwned;
    m_y         = m_yOwned;
    m_isUniform = false;
    m_isView    = false;
    refresh();
}

void SeriesData::setOwned(UniformX x, std::vector<double> y)
{
    m_xOwned.clear();
    m_xOwned.shrink_to_fit();
    m_yOwned    = std::move(y);
    m_x         = {};
    m_y         = m_yOwned;
    m_uniform   = x;
    m_isUniform = true;
    m_isView    = false;
    refresh();
}

void SeriesData::setView(std::span<const double> x, std::span<const double> y)
{
    requireSameSize(x.size(), y.size());
    m_xOwned    = {};
    m_yOwned    = {};
    m_x         = x;
    m_y         = y;
    m_isUniform = false;
    m_isView    = true;
    refresh();
}

void SeriesData::setView(UniformX x, std::span<const double> y)
{
    m_xOwned    = {};
    m_yOwned    = {};
    m_x         = {};
    m_y         = y;
    m_uniform   = x;
    m_isUniform = true;
    m_isView    = true;
    refresh();
}

void SeriesData::clear()
{
    setOwned(std::vector<double>{}, std::vector<double>{});
}

void SeriesData::refresh()
{
    resetDerived();
    updateDerived(0);
}

void SeriesData::append(std::span<const double> x, std::span<const double> y)
{
    if (m_isView || m_isUniform)
    {
        throw std::logic_error(
            "rocketplot: append(x, y) needs a series that owns its data and has an x array");
    }
    requireSameSize(x.size(), y.size());
    const std::size_t oldSize = size();
    m_xOwned.insert(m_xOwned.end(), x.begin(), x.end());
    m_yOwned.insert(m_yOwned.end(), y.begin(), y.end());
    m_x = m_xOwned;  // the vectors may have reallocated
    m_y = m_yOwned;
    updateDerived(oldSize);
}

void SeriesData::append(std::span<const double> y)
{
    if (m_isView || !m_isUniform)
    {
        throw std::logic_error(
            "rocketplot: append(y) needs a series that owns its data and has UniformX x values");
    }
    const std::size_t oldSize = size();
    m_yOwned.insert(m_yOwned.end(), y.begin(), y.end());
    m_y = m_yOwned;
    updateDerived(oldSize);
}

std::size_t SeriesData::lowerBound(double value) const
{
    if (!m_isUniform)
    {
        return static_cast<std::size_t>(
            std::ranges::distance(m_x.begin(), std::ranges::lower_bound(m_x, value)));
    }
    const std::size_t n = size();
    if (n == 0 || value <= m_uniform.start)
    {
        return 0;
    }
    if (m_uniform.step <= 0.0)
    {
        return n;  // every x equals start, which is < value
    }
    // Estimate arithmetically, then correct for rounding against x() itself.
    const double estimate = std::ceil((value - m_uniform.start) / m_uniform.step);
    std::size_t  index =
        estimate >= static_cast<double>(n) ? n : static_cast<std::size_t>(std::max(estimate, 0.0));
    while (index > 0 && x(index - 1) >= value)
    {
        --index;
    }
    while (index < n && x(index) < value)
    {
        ++index;
    }
    return index;
}

std::size_t SeriesData::upperBound(double value) const
{
    if (!m_isUniform)
    {
        return static_cast<std::size_t>(
            std::ranges::distance(m_x.begin(), std::ranges::upper_bound(m_x, value)));
    }
    const std::size_t n = size();
    if (n == 0 || value < m_uniform.start)
    {
        return 0;
    }
    if (m_uniform.step <= 0.0)
    {
        return n;
    }
    const double estimate = std::floor((value - m_uniform.start) / m_uniform.step) + 1.0;
    std::size_t  index =
        estimate >= static_cast<double>(n) ? n : static_cast<std::size_t>(std::max(estimate, 0.0));
    while (index > 0 && x(index - 1) > value)
    {
        --index;
    }
    while (index < n && x(index) <= value)
    {
        ++index;
    }
    return index;
}

Range SeriesData::yBoundsWithin(Range xRange, bool positiveOnly) const
{
    if (empty() || !(xRange.min <= xRange.max))
    {
        return Range::empty();
    }
    if (!m_sorted)
    {
        Range bounds = Range::empty();
        for (std::size_t i = 0; i < size(); ++i)
        {
            const double yi = m_y[i];
            if (xRange.contains(x(i)) && (!positiveOnly || yi > 0.0))
            {
                bounds = bounds.including(yi);
            }
        }
        return bounds;
    }
    const std::size_t first = lowerBound(xRange.min);
    const std::size_t last  = upperBound(xRange.max);
    if (first >= last)
    {
        return Range::empty();
    }
    const MinMax summary = m_pyramid.query(m_y, first, last);
    if (!summary.hasFinite())
    {
        return Range::empty();
    }
    if (positiveOnly)
    {
        return summary.max > 0.0 ? Range{.min = summary.minPositive, .max = summary.max}
                                 : Range::empty();
    }
    return {.min = summary.min, .max = summary.max};
}

void SeriesData::resetDerived()
{
    m_sorted    = m_isUniform ? isSortedUniform(m_uniform) : true;
    m_xBounds   = Range::empty();
    m_yBounds   = Range::empty();
    m_xPositive = Range::empty();
    m_yPositive = Range::empty();
    m_pyramid.clear();
}

void SeriesData::updateDerived(std::size_t first)
{
    const std::size_t n = size();
    for (std::size_t i = first; i < n; ++i)
    {
        const double xi = x(i);
        const double yi = m_y[i];
        if (!m_isUniform && m_sorted && (std::isnan(xi) || (i > 0 && xi < m_x[i - 1])))
        {
            m_sorted = false;
        }
        if (std::isfinite(xi) && std::isfinite(yi))
        {
            m_xBounds = m_xBounds.including(xi);
            m_yBounds = m_yBounds.including(yi);
            if (xi > 0.0)
            {
                m_xPositive = m_xPositive.including(xi);
            }
            if (yi > 0.0)
            {
                m_yPositive = m_yPositive.including(yi);
            }
        }
    }
    m_pyramid.extend(m_y, first);
}

}  // namespace rocketplot::core
