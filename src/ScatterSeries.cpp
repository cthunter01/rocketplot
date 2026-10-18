#include "rocketplot/ScatterSeries.h"

#include <QColor>
#include <QRgb>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TextPainter.h"
#include "core/CsvWriter.h"
#include "core/NearestPoint.h"
#include "rocketplot/ColorScale.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
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

ScatterSeries::ScatterSeries(PlotWidget* plot)
  : Series(plot, SeriesKind::SCATTER, Marker::CIRCLE, ErrorStyle::BARS)
{
}

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
    if (!m_colorValues.empty())
    {
        m_colorValues.clear();
        m_colorBounds = Range::empty();
        Q_EMIT dataChanged();  // the color scale fits what is left
    }
    Q_EMIT changed();
}

void ScatterSeries::setColorValues(std::vector<double>&& values)
{
    requireOnePerPoint("color values", values.size(), size());
    m_colorValues = std::move(values);
    m_colors.clear();
    m_colorBounds = Range::empty();
    for (const double value : m_colorValues)
    {
        if (std::isfinite(value))
        {
            m_colorBounds = m_colorBounds.including(value);
        }
    }
    Q_EMIT dataChanged();  // the color scale fits them
    Q_EMIT changed();
}

void ScatterSeries::clearColorValues()
{
    if (m_colorValues.empty())
    {
        return;
    }
    m_colorValues.clear();
    m_colorBounds = Range::empty();
    Q_EMIT dataChanged();
    Q_EMIT changed();
}

double ScatterSeries::colorValue(std::size_t index) const
{
    return index < m_colorValues.size() ? m_colorValues[index]
                                        : std::numeric_limits<double>::quiet_NaN();
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
    if (index < m_colorValues.size())
    {
        return plot()->colorScale()->colorAt(m_colorValues[index]);
    }
    return index < m_colors.size() ? QColor::fromRgba(m_colors[index]) : color();
}

void ScatterSeries::addCsvColumns(core::CsvSeries& columns) const
{
    if (m_colorValues.empty())
    {
        return;
    }
    const QString label = plainText(plot()->colorScale()->label());
    columns.columns.push_back(
        {.suffix = "(" + (label.isEmpty() ? std::string("color value") : label.toStdString()) + ")",
         .values = m_colorValues});
}

bool ScatterSeries::pick(const core::PickQuery& query, core::NearestPoint& found) const
{
    // A point with its own size of 0 is hidden: it can't be landed on.
    const std::optional<core::NearestPoint> point =
        query.tracing
            ? core::tracedPoint(data(), *query.x, *query.y, query.box, query.position, m_sizes)
            : core::nearestPoint(data(), *query.x, *query.y, query.box, query.position, query.reach,
                                 m_sizes);
    if (point)
    {
        found = *point;
    }
    return point.has_value();
}

void ScatterSeries::pointsReplaced()
{
    m_sizes.clear();
    m_colors.clear();
    m_colorValues.clear();
    m_colorBounds = Range::empty();
    m_largestSize = 0.0;
}

}  // namespace rocketplot
