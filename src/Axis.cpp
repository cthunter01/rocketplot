#include "rocketplot/Axis.h"

#include <QObject>
#include <QString>
#include <Qt>
#include <cmath>
#include <utility>

#include "Logging.h"
#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

Axis::Axis(Qt::Orientation orientation, QObject* parent)
  : QObject(parent), m_orientation(orientation)
{
}

Axis::~Axis() = default;

void Axis::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

void Axis::setRange(Range range)
{
    if (range.min > range.max)
    {
        std::swap(range.min, range.max);
    }
    if (!core::isUsableRange(range))
    {
        qCWarning(lcData) << "Axis::setRange: ignoring a range that cannot be shown:" << range.min
                          << range.max;
        return;
    }
    const bool autoscaleChanged = m_autoscale;
    m_autoscale                 = false;
    if (range == m_range)
    {
        if (autoscaleChanged)
        {
            Q_EMIT changed();
        }
        return;
    }
    applyRange(range);
}

void Axis::applyRange(Range range)
{
    if (range == m_range)
    {
        return;
    }
    m_range = range;
    Q_EMIT rangeChanged(m_range.min, m_range.max);
    Q_EMIT changed();
}

void Axis::setAutoscale(bool enabled)
{
    if (enabled == m_autoscale)
    {
        return;
    }
    m_autoscale = enabled;
    Q_EMIT changed();
    if (enabled)
    {
        Q_EMIT autoscaleEnabled();
    }
}

void Axis::setAutoscaleMargin(double margin)
{
    margin = std::isfinite(margin) && margin > 0.0 ? margin : 0.0;
    if (margin == m_autoscaleMargin)
    {
        return;
    }
    m_autoscaleMargin = margin;
    Q_EMIT changed();
    if (m_autoscale)
    {
        Q_EMIT autoscaleEnabled();
    }
}

void Axis::setGridVisible(bool visible)
{
    if (visible == m_gridVisible)
    {
        return;
    }
    m_gridVisible = visible;
    Q_EMIT changed();
}

}  // namespace rocketplot
