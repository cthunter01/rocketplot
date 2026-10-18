#include "rocketplot/HistogramSeries.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotState.h"
#include "core/AxisMapping.h"
#include "core/Histogram.h"
#include "rocketplot/BarSeries.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

core::HistogramScale coreScale(HistogramNormalization normalization)
{
    switch (normalization)
    {
        case HistogramNormalization::COUNT:
            break;
        case HistogramNormalization::PROBABILITY:
            return core::HistogramScale::PROBABILITY;
        case HistogramNormalization::DENSITY:
            return core::HistogramScale::DENSITY;
    }
    return core::HistogramScale::COUNT;
}

}  // namespace

HistogramSeries::HistogramSeries(PlotWidget* plot) : BarSeries(plot) { }

HistogramSeries::~HistogramSeries() = default;

void HistogramSeries::setValues(std::vector<double>&& values)
{
    m_values = std::move(values);
    recount();
}

Range HistogramSeries::binRange() const noexcept
{
    if (m_binRange.isValid() || edges().empty())
    {
        return m_binRange;
    }
    return {.min = edges().front(), .max = edges().back()};
}

void HistogramSeries::setBinCount(int count)
{
    if (count < 0)
    {
        qCWarning(lcData) << "HistogramSeries::setBinCount: ignoring" << count;
        return;
    }
    if (count == m_binCount)
    {
        return;
    }
    m_binCount = count;
    recount();
    Q_EMIT changed();
}

void HistogramSeries::setBinRange(Range range)
{
    if (!core::isUsableRange(range))
    {
        qCWarning(lcData) << "HistogramSeries::setBinRange: ignoring" << range.min << range.max;
        return;
    }
    if (range == m_binRange)
    {
        return;
    }
    m_binRange = range;
    recount();
    Q_EMIT changed();
}

void HistogramSeries::resetBinRange()
{
    if (!m_binRange.isValid())
    {
        return;
    }
    m_binRange = Range::empty();
    recount();
    Q_EMIT changed();
}

void HistogramSeries::setNormalization(HistogramNormalization normalization)
{
    if (normalization == m_normalization)
    {
        return;
    }
    m_normalization = normalization;
    recount();
    Q_EMIT changed();
}

void HistogramSeries::recount()
{
    const Range range   = m_binRange.isValid() ? m_binRange : core::histogramRange(m_values);
    const auto  counted = static_cast<std::size_t>(
        std::ranges::count_if(m_values, [&range](double value) { return range.contains(value); }));
    const std::size_t bins =
        m_binCount > 0 ? static_cast<std::size_t>(m_binCount) : core::automaticBinCount(counted);
    core::Histogram histogram = core::histogram(m_values, bins, range, coreScale(m_normalization));
    applyBars(std::move(histogram.edges), std::move(histogram.heights));
}

void HistogramSeries::saveState(QJsonObject& state) const
{
    BarSeries::saveState(state);
    state.insert(QLatin1String("binCount"), m_binCount);
    state.insert(QLatin1String("binRange"),
                 m_binRange.isValid() ? QJsonValue(QJsonArray{m_binRange.min, m_binRange.max})
                                      : QJsonValue(QJsonValue::Null));
    state.insert(QLatin1String("normalization"), state::fromEnum(m_normalization));
}

void HistogramSeries::restoreState(const QJsonObject& state)
{
    BarSeries::restoreState(state);
    if (const auto count = state::toNumber(state.value(QLatin1String("binCount"))))
    {
        setBinCount(static_cast<int>(*count));
    }
    if (const QJsonValue range = state.value(QLatin1String("binRange")); range.isNull())
    {
        resetBinRange();
    }
    else if (const QJsonArray ends = range.toArray(); ends.size() == 2)
    {
        const auto min = state::toNumber(ends.at(0));
        const auto max = state::toNumber(ends.at(1));
        if (min && max)
        {
            setBinRange({.min = *min, .max = *max});
        }
    }
    if (const auto normalization =
            state::toEnum<HistogramNormalization>(state.value(QLatin1String("normalization"))))
    {
        setNormalization(*normalization);
    }
}

}  // namespace rocketplot
