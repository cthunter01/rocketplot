#include "PlotState.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonValue>
#include <QPointF>
#include <QString>
#include <cmath>
#include <optional>

namespace rocketplot::state
{

QJsonValue fromColor(const QColor& color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

std::optional<QColor> toColor(const QJsonValue& value)
{
    if (!value.isString())
    {
        return std::nullopt;
    }
    const QColor color = QColor::fromString(value.toString());
    return color.isValid() ? std::optional(color) : std::nullopt;
}

QJsonValue fromPoint(QPointF point)
{
    return QJsonArray{point.x(), point.y()};
}

std::optional<QPointF> toPoint(const QJsonValue& value)
{
    const QJsonArray coordinates = value.toArray();
    if (coordinates.size() != 2)
    {
        return std::nullopt;
    }
    const std::optional<double> x = toNumber(coordinates.at(0));
    const std::optional<double> y = toNumber(coordinates.at(1));
    return x && y ? std::optional(QPointF(*x, *y)) : std::nullopt;
}

std::optional<double> toNumber(const QJsonValue& value)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble()))
    {
        return std::nullopt;
    }
    return value.toDouble();
}

std::optional<bool> toBool(const QJsonValue& value)
{
    return value.isBool() ? std::optional(value.toBool()) : std::nullopt;
}

}  // namespace rocketplot::state
