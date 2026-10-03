#include "rocketplot/ScatterSeries.h"

#include <QColor>
#include <QRgb>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

void requireOnePerPoint(const char* what, std::size_t count, std::size_t points)
{
    if (count != points)
    {
        throw std::invalid_argument(
            std::format("rocketplot: {} must have one value per point (the series has {} points, "
                        "the {} {})",
                        what, points, what, count));
    }
}

}  // namespace

ScatterSeries::ScatterSeries(PlotWidget* plot) : Series(plot, Marker::CIRCLE, ErrorStyle::BARS) { }

ScatterSeries::~ScatterSeries() = default;

void ScatterSeries::setSizes(std::vector<double>&& sizes)
{
    requireOnePerPoint("sizes", sizes.size(), size());
    m_sizes       = std::move(sizes);
    m_largestSize = 0.0;
    for (double& value : m_sizes)
    {
        value         = std::isfinite(value) && value > 0.0 ? value : 0.0;
        m_largestSize = std::max(m_largestSize, value);
    }
    Q_EMIT changed();
}

void ScatterSeries::clearSizes()
{
    if (m_sizes.empty())
    {
        return;
    }
    m_sizes.clear();
    m_largestSize = 0.0;
    Q_EMIT changed();
}

double ScatterSeries::pointSize(std::size_t index) const
{
    return index < m_sizes.size() ? m_sizes[index] : markerSize();
}

void ScatterSeries::applyColors(std::vector<QRgb>&& colors)
{
    requireOnePerPoint("colors", colors.size(), size());
    m_colors = std::move(colors);
    Q_EMIT changed();
}

void ScatterSeries::clearColors()
{
    if (m_colors.empty())
    {
        return;
    }
    m_colors.clear();
    Q_EMIT changed();
}

QColor ScatterSeries::pointColor(std::size_t index) const
{
    return index < m_colors.size() ? QColor::fromRgba(m_colors[index]) : color();
}

void ScatterSeries::pointsReplaced()
{
    m_sizes.clear();
    m_colors.clear();
    m_largestSize = 0.0;
}

}  // namespace rocketplot
