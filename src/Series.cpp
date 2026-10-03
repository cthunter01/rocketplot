#include "rocketplot/Series.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Logging.h"
#include "core/ErrorData.h"
#include "core/SeriesData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

Series::Series(PlotWidget* plot, Marker marker, ErrorStyle errorStyle)
  : QObject(plot),
    m_plot(plot),
    m_data(std::make_unique<core::SeriesData>()),
    m_errors(std::make_unique<core::ErrorData>()),
    m_marker(marker),
    m_errorStyle(errorStyle)
{
}

Series::~Series() = default;

void Series::setName(const QString& name)
{
    if (name == m_name)
    {
        return;
    }
    m_name = name;
    Q_EMIT changed();
}

void Series::setVisible(bool visible)
{
    if (visible == m_visible)
    {
        return;
    }
    m_visible = visible;
    Q_EMIT changed();
}

QColor Series::color() const
{
    if (m_color)
    {
        return *m_color;
    }
    return m_plot->theme().seriesColor(m_colorIndex);
}

void Series::setColor(const QColor& color)
{
    if (!color.isValid())
    {
        resetColor();
        return;
    }
    if (m_color == color)
    {
        return;
    }
    m_color = color;
    Q_EMIT changed();
}

void Series::resetColor()
{
    if (!m_color)
    {
        return;
    }
    m_color.reset();
    Q_EMIT changed();
}

void Series::setMarker(Marker marker)
{
    if (marker == m_marker)
    {
        return;
    }
    m_marker = marker;
    Q_EMIT changed();
}

double Series::markerSize() const
{
    return m_markerSize.value_or(m_plot->theme().markerSize);
}

void Series::setMarkerSize(double size)
{
    if (!std::isfinite(size) || size <= 0.0)
    {
        qCWarning(lcData) << "Series::setMarkerSize: ignoring" << size;
        return;
    }
    if (m_markerSize == size)
    {
        return;
    }
    m_markerSize = size;
    Q_EMIT changed();
}

void Series::resetMarkerSize()
{
    if (!m_markerSize)
    {
        return;
    }
    m_markerSize.reset();
    Q_EMIT changed();
}

Axis* Series::yAxis() const
{
    return m_secondaryYAxis ? m_plot->yAxis2() : m_plot->yAxis();
}

void Series::setYAxis(Axis* axis)
{
    if (axis != m_plot->yAxis() && axis != m_plot->yAxis2())
    {
        qCWarning(lcData) << "Series::setYAxis: not a y axis of this series' plot";
        return;
    }
    setOnSecondaryYAxis(axis == m_plot->yAxis2());
}

void Series::setOnSecondaryYAxis(bool secondary)
{
    if (secondary == m_secondaryYAxis)
    {
        return;
    }
    m_secondaryYAxis = secondary;
    Q_EMIT changed();
}

std::size_t Series::size() const noexcept
{
    return m_data->size();
}

double Series::x(std::size_t index) const noexcept
{
    return m_data->x(index);
}

double Series::y(std::size_t index) const noexcept
{
    return m_data->y(index);
}

Range Series::xBounds() const noexcept
{
    return m_data->xBounds();
}

Range Series::yBounds() const noexcept
{
    return m_data->yBounds();
}

bool Series::isSortedByX() const noexcept
{
    return m_data->isSortedByX();
}

std::optional<std::size_t> Series::nearestIndex(double x) const
{
    return m_data->nearestIndex(x);
}

bool Series::hasUniformX() const noexcept
{
    return m_data->isUniform();
}

bool Series::isView() const noexcept
{
    return m_data->isView();
}

void Series::setData(std::vector<double>&& x, std::vector<double>&& y)
{
    m_data->setOwned(std::move(x), std::move(y));
    dataWasReplaced();
}

void Series::setData(UniformX x, std::vector<double>&& y)
{
    m_data->setOwned(x, std::move(y));
    dataWasReplaced();
}

void Series::setDataView(std::span<const double> x, std::span<const double> y)
{
    m_data->setView(x, y);
    dataWasReplaced();
}

void Series::setDataView(UniformX x, std::span<const double> y)
{
    m_data->setView(x, y);
    dataWasReplaced();
}

void Series::notifyDataChanged()
{
    m_data->refresh();
    dataWasReplaced();
}

void Series::append(double x, double y)
{
    appendPoints(std::span<const double>(&x, 1), std::span<const double>(&y, 1));
}

void Series::append(double y)
{
    appendSamples(std::span<const double>(&y, 1));
}

void Series::clear()
{
    m_data->clear();
    dataWasReplaced();
}

void Series::appendPoints(std::span<const double> x, std::span<const double> y)
{
    m_data->append(x, y);
    dataWasChanged();
}

void Series::appendSamples(std::span<const double> y)
{
    m_data->append(y);
    dataWasChanged();
}

void Series::applyXErrors(std::span<const double> minus, std::span<const double> plus)
{
    m_errors->setX(*m_data, minus, plus);
    Q_EMIT dataChanged();
}

void Series::applyYErrors(std::span<const double> minus, std::span<const double> plus)
{
    m_errors->setY(*m_data, minus, plus);
    Q_EMIT dataChanged();
}

void Series::clearErrors()
{
    if (m_errors->empty())
    {
        return;
    }
    m_errors->clear();
    Q_EMIT dataChanged();
}

bool Series::hasXErrors() const noexcept
{
    return m_errors->hasX();
}

bool Series::hasYErrors() const noexcept
{
    return m_errors->hasY();
}

Range Series::xErrorRange(std::size_t index) const noexcept
{
    if (index < m_errors->xLow().size())
    {
        return {.min = m_errors->xLow()[index], .max = m_errors->xHigh()[index]};
    }
    return {.min = x(index), .max = x(index)};
}

Range Series::yErrorRange(std::size_t index) const noexcept
{
    if (index < m_errors->yLow().size())
    {
        return {.min = m_errors->yLow()[index], .max = m_errors->yHigh()[index]};
    }
    return {.min = y(index), .max = y(index)};
}

void Series::setErrorStyle(ErrorStyle style)
{
    if (style == m_errorStyle)
    {
        return;
    }
    m_errorStyle = style;
    Q_EMIT changed();
}

double Series::errorCapSize() const
{
    return m_errorCapSize.value_or(m_plot->theme().errorCapSize);
}

void Series::setErrorCapSize(double size)
{
    if (!std::isfinite(size) || size < 0.0)
    {
        qCWarning(lcData) << "Series::setErrorCapSize: ignoring" << size;
        return;
    }
    if (m_errorCapSize == size)
    {
        return;
    }
    m_errorCapSize = size;
    Q_EMIT changed();
}

void Series::resetErrorCapSize()
{
    if (!m_errorCapSize)
    {
        return;
    }
    m_errorCapSize.reset();
    Q_EMIT changed();
}

double Series::bandOpacity() const
{
    return m_bandOpacity.value_or(m_plot->theme().bandOpacity);
}

void Series::setBandOpacity(double opacity)
{
    if (!std::isfinite(opacity))
    {
        qCWarning(lcData) << "Series::setBandOpacity: ignoring" << opacity;
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (m_bandOpacity == opacity)
    {
        return;
    }
    m_bandOpacity = opacity;
    Q_EMIT changed();
}

void Series::resetBandOpacity()
{
    if (!m_bandOpacity)
    {
        return;
    }
    m_bandOpacity.reset();
    Q_EMIT changed();
}

Range Series::fitBoundsX(bool positiveOnly) const
{
    return positiveOnly ? m_data->xPositiveBounds().united(m_errors->xPositiveBounds())
                        : m_data->xBounds().united(m_errors->xBounds());
}

Range Series::fitBoundsY(bool positiveOnly) const
{
    return positiveOnly ? m_data->yPositiveBounds().united(m_errors->yPositiveBounds())
                        : m_data->yBounds().united(m_errors->yBounds());
}

Range Series::fitBoundsYWithin(Range xRange, bool positiveOnly) const
{
    return m_data->yBoundsWithin(xRange, positiveOnly)
        .united(m_errors->yBoundsWithin(*m_data, xRange, positiveOnly));
}

void Series::dataWasReplaced()
{
    m_errors->clear();
    pointsReplaced();
    dataWasChanged();
}

void Series::pointsReplaced() { }

void Series::dataWasChanged()
{
    qCDebug(lcData) << "series" << m_name << "has" << m_data->size()
                    << "points, sorted:" << m_data->isSortedByX() << "view:" << m_data->isView();
    Q_EMIT dataChanged();
}

}  // namespace rocketplot
