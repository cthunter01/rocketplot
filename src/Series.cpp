#include "rocketplot/Series.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Logging.h"
#include "core/SeriesData.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

Series::Series(PlotWidget* plot, Marker marker)
  : QObject(plot), m_plot(plot), m_data(std::make_unique<core::SeriesData>()), m_marker(marker)
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
    dataWasChanged();
}

void Series::setData(UniformX x, std::vector<double>&& y)
{
    m_data->setOwned(x, std::move(y));
    dataWasChanged();
}

void Series::setDataView(std::span<const double> x, std::span<const double> y)
{
    m_data->setView(x, y);
    dataWasChanged();
}

void Series::setDataView(UniformX x, std::span<const double> y)
{
    m_data->setView(x, y);
    dataWasChanged();
}

void Series::notifyDataChanged()
{
    m_data->refresh();
    dataWasChanged();
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
    dataWasChanged();
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

void Series::dataWasChanged()
{
    qCDebug(lcData) << "series" << m_name << "has" << m_data->size()
                    << "points, sorted:" << m_data->isSortedByX() << "view:" << m_data->isView();
    Q_EMIT dataChanged();
}

}  // namespace rocketplot
