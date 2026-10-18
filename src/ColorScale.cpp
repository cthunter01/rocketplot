#include "rocketplot/ColorScale.h"

#include <QColor>
#include <QGradientStops>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QList>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotState.h"
#include "core/AxisMapping.h"
#include "core/ColorMap.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

core::ColorTable tableOf(ColorMap map)
{
    switch (map)
    {
        case ColorMap::VIRIDIS:
        case ColorMap::CUSTOM:
            break;
        case ColorMap::PLASMA:
            return core::ColorTable::PLASMA;
        case ColorMap::INFERNO:
            return core::ColorTable::INFERNO;
        case ColorMap::MAGMA:
            return core::ColorTable::MAGMA;
        case ColorMap::CIVIDIS:
            return core::ColorTable::CIVIDIS;
        case ColorMap::GRAY:
            return core::ColorTable::GRAY;
        case ColorMap::COOL_WARM:
            return core::ColorTable::COOL_WARM;
        case ColorMap::BLUE_RED:
            return core::ColorTable::BLUE_RED;
    }
    return core::ColorTable::VIRIDIS;
}

std::vector<core::GradientStop> coreStops(const QGradientStops& stops)
{
    std::vector<core::GradientStop> converted;
    converted.reserve(static_cast<std::size_t>(stops.size()));
    for (const auto& [position, color] : stops)
    {
        const QColor rgb = color.toRgb();
        converted.push_back({.position = position,
                             .color    = {.r = static_cast<std::uint8_t>(rgb.red()),
                                          .g = static_cast<std::uint8_t>(rgb.green()),
                                          .b = static_cast<std::uint8_t>(rgb.blue()),
                                          .a = static_cast<std::uint8_t>(rgb.alpha())}});
    }
    return converted;
}

QRgb toRgb(core::Rgba color)
{
    return qRgba(color.r, color.g, color.b, color.a);
}

}  // namespace

ColorScale::ColorScale(PlotWidget* plot) : QObject(plot), m_plot(plot) { }

ColorScale::~ColorScale() = default;

bool ColorScale::isVisible() const
{
    if (m_visible)
    {
        return *m_visible;
    }
    const QList<Series*> all = m_plot->series();
    return std::ranges::any_of(all, [](const Series* series) {
        const auto* scatter = qobject_cast<const ScatterSeries*>(series);
        return scatter != nullptr && scatter->isVisible() && scatter->hasColorValues();
    });
}

void ColorScale::setVisible(bool visible)
{
    if (m_visible == visible)
    {
        return;
    }
    m_visible = visible;
    Q_EMIT changed();
}

void ColorScale::resetVisible()
{
    if (!m_visible)
    {
        return;
    }
    m_visible.reset();
    Q_EMIT changed();
}

void ColorScale::setRange(Range range)
{
    if (range.min > range.max)
    {
        std::swap(range.min, range.max);
    }
    if (!core::isUsableRange(range))
    {
        qCWarning(lcData) << "ColorScale::setRange: ignoring" << range.min << range.max;
        return;
    }
    const bool wasAutoscaling = std::exchange(m_autoscale, false);
    if (range != m_range)
    {
        applyRange(range);
    }
    else if (wasAutoscaling)
    {
        Q_EMIT changed();
    }
}

void ColorScale::applyRange(Range range)
{
    if (range == m_range)
    {
        return;
    }
    m_range = range;
    Q_EMIT rangeChanged(m_range.min, m_range.max);
    Q_EMIT changed();
}

void ColorScale::setAutoscale(bool enabled)
{
    if (enabled == m_autoscale)
    {
        return;
    }
    m_autoscale = enabled;
    Q_EMIT changed();  // the plot fits the values again
}

void ColorScale::setColorMap(ColorMap map)
{
    if (map == m_colorMap)
    {
        return;
    }
    m_colorMap = map;
    Q_EMIT changed();
}

void ColorScale::setGradient(const QGradientStops& stops)
{
    if (stops.isEmpty())
    {
        qCWarning(lcData) << "ColorScale::setGradient: ignoring a gradient without stops";
        return;
    }
    QGradientStops sorted = stops;
    for (auto& stop : sorted)
    {
        stop.first = std::clamp(stop.first, 0.0, 1.0);
    }
    std::ranges::stable_sort(sorted, {}, [](const auto& stop) { return stop.first; });
    if (sorted == m_stops && m_colorMap == ColorMap::CUSTOM)
    {
        return;
    }
    m_stops    = std::move(sorted);
    m_colorMap = ColorMap::CUSTOM;
    Q_EMIT changed();
}

QGradientStops ColorScale::gradient() const
{
    if (m_colorMap == ColorMap::CUSTOM)
    {
        return m_stops;
    }
    // A built-in map as stops, close enough to draw it with.
    constexpr int  kStops = 16;
    QGradientStops stops;
    for (int i = 0; i <= kStops; ++i)
    {
        const double t = static_cast<double>(i) / kStops;
        stops.append({t, QColor::fromRgba(toRgb(core::tableAt(tableOf(m_colorMap), t)))});
    }
    return stops;
}

QColor ColorScale::nanColor() const
{
    return m_nanColor.value_or(m_plot->theme().secondaryText);
}

void ColorScale::setNanColor(const QColor& color)
{
    if (m_nanColor == color)
    {
        return;
    }
    m_nanColor = color;
    Q_EMIT changed();
}

void ColorScale::resetNanColor()
{
    if (!m_nanColor)
    {
        return;
    }
    m_nanColor.reset();
    Q_EMIT changed();
}

void ColorScale::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

void ColorScale::setNumberFormat(NumberFormat format)
{
    if (format == m_numberFormat)
    {
        return;
    }
    m_numberFormat = format;
    Q_EMIT changed();
}

QColor ColorScale::colorAt(double value) const
{
    if (std::isnan(value))
    {
        return nanColor();
    }
    const double t = (value - m_range.min) / m_range.span();
    if (m_colorMap == ColorMap::CUSTOM)
    {
        const std::vector<core::GradientStop> stops = coreStops(m_stops);
        return QColor::fromRgba(toRgb(core::gradientAt(stops, t)));
    }
    return QColor::fromRgba(toRgb(core::tableAt(tableOf(m_colorMap), t)));
}

ColorScale::Lookup ColorScale::lookup() const
{
    const core::Lut lut = m_colorMap == ColorMap::CUSTOM ? core::buildLut(coreStops(m_stops))
                                                         : core::buildLut(tableOf(m_colorMap));
    Lookup          colors{};
    for (std::size_t i = 0; i < colors.size(); ++i)
    {
        colors.at(i) = toRgb(lut.at(i));
    }
    return colors;
}

void ColorScale::saveState(QJsonObject& state) const
{
    state.insert(QLatin1String("visible"), state::fromOptional(m_visible));
    state.insert(QLatin1String("min"), m_range.min);
    state.insert(QLatin1String("max"), m_range.max);
    state.insert(QLatin1String("autoscale"), m_autoscale);
    if (m_colorMap != ColorMap::CUSTOM)  // a gradient of the code's own comes from the code
    {
        state.insert(QLatin1String("colorMap"), state::fromEnum(m_colorMap));
    }
    state.insert(QLatin1String("nanColor"),
                 m_nanColor ? state::fromColor(*m_nanColor) : QJsonValue(QJsonValue::Null));
    state.insert(QLatin1String("numberFormat"), state::fromEnum(m_numberFormat));
}

void ColorScale::restoreState(const QJsonObject& state)
{
    state::restoreOptional(
        state.value(QLatin1String("visible")), state::toBool,
        [this](bool visible) { setVisible(visible); }, [this] { resetVisible(); });
    if (const auto map = state::toEnum<ColorMap>(state.value(QLatin1String("colorMap")));
        map && *map != ColorMap::CUSTOM)
    {
        setColorMap(*map);
    }
    state::restoreOptional(
        state.value(QLatin1String("nanColor")), state::toColor,
        [this](const QColor& color) { setNanColor(color); }, [this] { resetNanColor(); });
    if (const auto format = state::toEnum<NumberFormat>(state.value(QLatin1String("numberFormat"))))
    {
        setNumberFormat(*format);
    }
    // A fixed range, or autoscale (which fits the values the plot has now).
    const auto autoscale = state::toBool(state.value(QLatin1String("autoscale")));
    const auto min       = state::toNumber(state.value(QLatin1String("min")));
    const auto max       = state::toNumber(state.value(QLatin1String("max")));
    if (autoscale.value_or(false))
    {
        setAutoscale(true);
    }
    else if (min && max)
    {
        setRange(*min, *max);
    }
}

}  // namespace rocketplot
