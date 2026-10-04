#include "rocketplot/Axis.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QObject>
#include <QString>
#include <QTimeZone>
#include <Qt>
#include <cmath>
#include <optional>
#include <utility>

#include "Logging.h"
#include "PlotState.h"
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

void Axis::saveState(QJsonObject& state) const
{
    state.insert(QLatin1String("min"), m_range.min);
    state.insert(QLatin1String("max"), m_range.max);
    state.insert(QLatin1String("autoscale"), m_autoscale);
    state.insert(QLatin1String("autoscaleMode"), state::fromEnum(m_autoscaleMode));
    state.insert(QLatin1String("autoscaleMargin"), m_autoscaleMargin);
    state.insert(QLatin1String("followWindow"), m_followWindow);
    state.insert(QLatin1String("scaleType"), state::fromEnum(m_scaleType));
    state.insert(QLatin1String("numberFormat"), state::fromEnum(m_numberFormat));
    state.insert(QLatin1String("timeZone"), QString::fromLatin1(m_timeZone.id()));
    state.insert(QLatin1String("gridVisible"), m_gridVisible);
    state.insert(QLatin1String("minorGridVisible"), m_minorGridVisible);
    state.insert(QLatin1String("visible"), state::fromOptional(m_visible));
}

void Axis::restoreState(const QJsonObject& state)
{
    // The scale first: it decides which ranges the axis can show.
    if (const auto scale = state::toEnum<ScaleType>(state.value(QLatin1String("scaleType"))))
    {
        setScaleType(*scale);
    }
    if (const auto format = state::toEnum<NumberFormat>(state.value(QLatin1String("numberFormat"))))
    {
        setNumberFormat(*format);
    }
    if (const QJsonValue zone = state.value(QLatin1String("timeZone")); zone.isString())
    {
        setTimeZone(QTimeZone(zone.toString().toLatin1()));  // ignored if there is no such zone
    }
    if (const auto mode = state::toEnum<AutoscaleMode>(state.value(QLatin1String("autoscaleMode"))))
    {
        setAutoscaleMode(*mode);
    }
    if (const auto margin = state::toNumber(state.value(QLatin1String("autoscaleMargin"))))
    {
        setAutoscaleMargin(*margin);
    }
    if (const auto window = state::toNumber(state.value(QLatin1String("followWindow"))))
    {
        setFollowWindow(*window);
    }
    if (const auto grid = state::toBool(state.value(QLatin1String("gridVisible"))))
    {
        setGridVisible(*grid);
    }
    if (const auto grid = state::toBool(state.value(QLatin1String("minorGridVisible"))))
    {
        setMinorGridVisible(*grid);
    }
    state::restoreOptional(
        state.value(QLatin1String("visible")), state::toBool,
        [this](bool visible) { setVisible(visible); }, [this] { resetVisible(); });

    // An axis that was following the data follows the data there is now; one that wasn't shows
    // the range it showed. A range without a word on autoscale is a range to show.
    const std::optional<bool>   autoscale = state::toBool(state.value(QLatin1String("autoscale")));
    const std::optional<double> min       = state::toNumber(state.value(QLatin1String("min")));
    const std::optional<double> max       = state::toNumber(state.value(QLatin1String("max")));
    if (autoscale.value_or(false))
    {
        applyAutoscale(true);
        Q_EMIT fitNeeded();
    }
    else if (min && max)
    {
        setRange(*min, *max);
    }
    else if (autoscale)
    {
        applyAutoscale(false);
    }
}

}  // namespace rocketplot
