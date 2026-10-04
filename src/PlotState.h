#pragma once

#include <QByteArray>
#include <QColor>
#include <QJsonValue>
#include <QLatin1String>
#include <QMetaEnum>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <optional>

// How settings are written in a saved state (PlotWidget::saveState()) and read back. Enumerators
// are written by name, colors as "#rrggbb" ("#aarrggbb" when translucent) and points as [x, y]. A
// setting that follows the theme or a default until it is set is null while it does.
//
// Reading gives nothing for a value of the wrong kind (a missing one too), which leaves the
// setting as it is: a state can be a part of one, or written by a later version.
namespace rocketplot::state
{

inline constexpr QLatin1String kFormat("rocketplot.state");
inline constexpr int           kVersion = 1;

template <class Enum>
[[nodiscard]] QJsonValue fromEnum(Enum value)
{
    // Through QVariant: QMetaEnum::valueToKey() takes an int up to Qt 6.8 and a quint64 after.
    return QVariant::fromValue(value).toString();
}

template <class Enum>
[[nodiscard]] std::optional<Enum> toEnum(const QJsonValue& value)
{
    if (!value.isString())
    {
        return std::nullopt;
    }
    bool             known = false;
    const QByteArray key   = value.toString().toLatin1();
    const int        found = QMetaEnum::fromType<Enum>().keyToValue(key.constData(), &known);
    return known ? std::optional(static_cast<Enum>(found)) : std::nullopt;
}

[[nodiscard]] QJsonValue            fromColor(const QColor& color);
[[nodiscard]] std::optional<QColor> toColor(const QJsonValue& value);

[[nodiscard]] QJsonValue             fromPoint(QPointF point);
[[nodiscard]] std::optional<QPointF> toPoint(const QJsonValue& value);

/// A finite number.
[[nodiscard]] std::optional<double> toNumber(const QJsonValue& value);
[[nodiscard]] std::optional<bool>   toBool(const QJsonValue& value);

/// The value of a setting that may be following a default: null while it is.
template <class T>
[[nodiscard]] QJsonValue fromOptional(const std::optional<T>& value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

/// Restores such a setting: null goes back to the default with @p reset, a value @p convert
/// understands is passed to @p set, and anything else leaves the setting alone.
template <class Convert, class Set, class Reset>
void restoreOptional(const QJsonValue& value, Convert convert, Set set, Reset reset)
{
    if (value.isNull())
    {
        reset();
    }
    else if (const auto converted = convert(value))
    {
        set(*converted);
    }
}

}  // namespace rocketplot::state
