#include "rocketplot/Axis.h"

#include <QObject>
#include <QString>
#include <QTimeZone>
#include <Qt>
#include <cmath>
#include <utility>

#include "Logging.h"
#include "core/AxisMapping.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

core::Scale coreScale(ScaleType type)
{
    return type == ScaleType::LOGARITHMIC ? core::Scale::LOG : core::Scale::LINEAR;
}

}  // namespace

Axis::Axis(Qt::Orientation orientation, bool secondary, QObject* parent)
  : QObject(parent),
    m_orientation(orientation),
    m_secondary(secondary),
    m_gridVisible(!secondary)  // one set of horizontal grid lines is enough
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
    if (!core::isUsableRange(range, coreScale(m_scaleType)))
    {
        qCWarning(lcData) << "Axis::setRange: ignoring a range this axis cannot show:" << range.min
                          << range.max;
        return;
    }
    applyAutoscale(false);
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

void Axis::applyAutoscale(bool enabled)
{
    if (enabled == m_autoscale)
    {
        return;
    }
    m_autoscale = enabled;
    Q_EMIT autoscaleChanged(enabled);
    Q_EMIT changed();
}

void Axis::setScaleType(ScaleType type)
{
    if (type == m_scaleType)
    {
        return;
    }
    m_scaleType = type;
    if (!core::isUsableRange(m_range, coreScale(type)))
    {
        applyAutoscale(true);
    }
    Q_EMIT changed();
    Q_EMIT fitNeeded();
}

void Axis::setNumberFormat(NumberFormat format)
{
    if (format == m_numberFormat)
    {
        return;
    }
    m_numberFormat = format;
    Q_EMIT changed();
}

void Axis::setTimeZone(const QTimeZone& zone)
{
    if (zone == m_timeZone || !zone.isValid())
    {
        return;
    }
    m_timeZone = zone;
    Q_EMIT changed();
}

void Axis::setAutoscale(bool enabled)
{
    if (enabled == m_autoscale)
    {
        return;
    }
    applyAutoscale(enabled);
    if (enabled)
    {
        Q_EMIT fitNeeded();
    }
}

void Axis::setAutoscaleMode(AutoscaleMode mode)
{
    if (mode == m_autoscaleMode)
    {
        return;
    }
    m_autoscaleMode = mode;
    Q_EMIT changed();
    Q_EMIT fitNeeded();
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
    Q_EMIT fitNeeded();
}

void Axis::setFollowWindow(double window)
{
    if (!std::isfinite(window) || window <= 0.0)
    {
        qCWarning(lcData) << "Axis::setFollowWindow: ignoring" << window;
        return;
    }
    if (window == m_followWindow)
    {
        return;
    }
    m_followWindow = window;
    Q_EMIT changed();
    Q_EMIT fitNeeded();
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

void Axis::setMinorGridVisible(bool visible)
{
    if (visible == m_minorGridVisible)
    {
        return;
    }
    m_minorGridVisible = visible;
    Q_EMIT changed();
}

void Axis::setVisible(bool visible)
{
    if (m_visible == visible)
    {
        return;
    }
    m_visible = visible;
    Q_EMIT changed();
}

void Axis::resetVisible()
{
    if (!m_visible)
    {
        return;
    }
    m_visible.reset();
    Q_EMIT changed();
}

bool Axis::isShown(bool hasSeries) const noexcept
{
    if (m_visible)
    {
        return *m_visible;
    }
    return !m_secondary || hasSeries;
}

}  // namespace rocketplot
