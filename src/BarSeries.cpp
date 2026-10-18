#include "rocketplot/BarSeries.h"

#include <QJsonObject>
#include <QLatin1String>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotState.h"
#include "core/Bars.h"
#include "core/CsvWriter.h"
#include "core/ErrorData.h"
#include "core/NearestPoint.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr double kMaxGap = 0.9;

// The positive part of @p range (for a log axis), or Range::empty().
Range positivePart(Range range)
{
    if (!range.isValid() || !(range.max > 0.0))
    {
        return Range::empty();
    }
    return {.min = std::max(range.min, std::nextafter(0.0, 1.0)), .max = range.max};
}

}  // namespace

BarSeries::BarSeries(PlotWidget* plot)
  : Series(plot, SeriesKind::BARS, Marker::NONE, ErrorStyle::BARS)
{
    lockData(false);
}

BarSeries::~BarSeries() = default;

void BarSeries::setBars(std::vector<double>&& edges, std::vector<double>&& heights)
{
    if (!takesBars())
    {
        throw std::logic_error(
            "rocketplot::BarSeries::setBars: a histogram's bars come from its "
            "values");
    }
    applyBars(std::move(edges), std::move(heights));
}

void BarSeries::applyBars(std::vector<double>&& edges, std::vector<double>&& heights)
{
    if (edges.empty() && heights.empty())
    {
        m_edges.clear();
        replacePoints({}, {});
        return;
    }
    if (edges.size() != heights.size() + 1)
    {
        throw std::invalid_argument("rocketplot::BarSeries::setBars: n bars need n + 1 edges");
    }
    for (std::size_t i = 0; i < edges.size(); ++i)
    {
        if (!std::isfinite(edges[i]) || (i > 0 && !(edges[i] > edges[i - 1])))
        {
            throw std::invalid_argument(
                "rocketplot::BarSeries::setBars: the edges must be finite and increasing");
        }
    }
    std::vector<double> middles(heights.size());
    for (std::size_t i = 0; i < middles.size(); ++i)
    {
        middles[i] = (edges[i] / 2.0) + (edges[i + 1] / 2.0);
    }
    m_edges = std::move(edges);
    replacePoints(std::move(middles), std::move(heights));
}

Range BarSeries::bin(std::size_t index) const
{
    if (index + 1 >= m_edges.size())
    {
        return Range::empty();
    }
    return {.min = m_edges[index], .max = m_edges[index + 1]};
}

std::optional<std::size_t> BarSeries::binAt(double x) const
{
    return core::binAt(m_edges, x);
}

void BarSeries::setFillOpacity(double opacity)
{
    if (!std::isfinite(opacity))
    {
        qCWarning(lcData) << "BarSeries::setFillOpacity: ignoring" << opacity;
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (opacity == m_fillOpacity)
    {
        return;
    }
    m_fillOpacity = opacity;
    Q_EMIT changed();
}

void BarSeries::setOutlineVisible(bool visible)
{
    if (visible == m_outlineVisible)
    {
        return;
    }
    m_outlineVisible = visible;
    Q_EMIT changed();
}

void BarSeries::setBarGap(double gap)
{
    if (!std::isfinite(gap))
    {
        qCWarning(lcData) << "BarSeries::setBarGap: ignoring" << gap;
        return;
    }
    gap = std::clamp(gap, 0.0, kMaxGap);
    if (gap == m_barGap)
    {
        return;
    }
    m_barGap = gap;
    Q_EMIT changed();
}

Range BarSeries::fitBoundsX(bool positiveOnly) const
{
    if (m_edges.empty())
    {
        return Range::empty();
    }
    const Range bins{.min = m_edges.front(), .max = m_edges.back()};
    return positiveOnly ? positivePart(bins).united(errors().xPositiveBounds())
                        : bins.united(errors().xBounds());
}

Range BarSeries::fitBoundsY(bool positiveOnly) const
{
    // Down to 0, where the bars stand; a log axis shows them from its bottom.
    if (positiveOnly)
    {
        return data().yPositiveBounds().united(errors().yPositiveBounds());
    }
    const Range heights = data().yBounds().united(errors().yBounds());
    return heights.isValid() ? heights.including(0.0) : heights;
}

Range BarSeries::fitBoundsYWithin(Range xRange, bool positiveOnly) const
{
    // The bars that reach into the range, whether or not their middles are in it.
    Range heights = Range::empty();
    for (std::size_t i = 0; i < size(); ++i)
    {
        const double height = y(i);
        if (m_edges[i + 1] < xRange.min || m_edges[i] > xRange.max || !std::isfinite(height) ||
            (positiveOnly && !(height > 0.0)))
        {
            continue;
        }
        heights = heights.including(height);
    }
    heights = heights.united(errors().yBoundsWithin(data(), xRange, positiveOnly));
    return heights.isValid() && !positiveOnly ? heights.including(0.0) : heights;
}

Qt::Orientations BarSeries::zeroBaselines() const
{
    return Qt::Vertical;
}

bool BarSeries::pick(const core::PickQuery& query, core::NearestPoint& found) const
{
    const std::optional<core::NearestPoint> bar = core::pickBar(m_edges, data().ys(), query);
    if (bar)
    {
        found = *bar;
    }
    return bar.has_value();
}

std::optional<std::size_t> BarSeries::indexNear(double x) const
{
    return binAt(x);
}

std::optional<double> BarSeries::yAt(double x) const
{
    const std::optional<std::size_t> bar = binAt(x);
    if (!bar || !std::isfinite(y(*bar)))
    {
        return std::nullopt;
    }
    return y(*bar);
}

void BarSeries::addCsvColumns(core::CsvSeries& columns) const
{
    if (m_edges.empty())
    {
        return;
    }
    const std::span<const double> edges(m_edges);
    columns.columns.push_back({.suffix = "(bin start)", .values = edges.first(edges.size() - 1)});
    columns.columns.push_back({.suffix = "(bin end)", .values = edges.subspan(1)});
}

void BarSeries::saveState(QJsonObject& state) const
{
    Series::saveState(state);
    state.insert(QLatin1String("fillOpacity"), m_fillOpacity);
    state.insert(QLatin1String("outlineVisible"), m_outlineVisible);
    state.insert(QLatin1String("barGap"), m_barGap);
}

void BarSeries::restoreState(const QJsonObject& state)
{
    Series::restoreState(state);
    if (const auto opacity = state::toNumber(state.value(QLatin1String("fillOpacity"))))
    {
        setFillOpacity(*opacity);
    }
    if (const auto outline = state::toBool(state.value(QLatin1String("outlineVisible"))))
    {
        setOutlineVisible(*outline);
    }
    if (const auto gap = state::toNumber(state.value(QLatin1String("barGap"))))
    {
        setBarGap(*gap);
    }
}

}  // namespace rocketplot
