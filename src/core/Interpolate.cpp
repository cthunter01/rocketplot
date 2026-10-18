#include "core/Interpolate.h"

#include <cmath>
#include <cstddef>
#include <optional>

#include "core/AxisMapping.h"
#include "core/SeriesData.h"

namespace rocketplot::core
{

namespace
{

// @p value in @p scale's space, or nothing where the scale can't show it.
std::optional<double> forward(double value, Scale scale)
{
    if (!std::isfinite(value) || (scale == Scale::LOG && !(value > 0.0)))
    {
        return std::nullopt;
    }
    return scale == Scale::LOG ? std::log10(value) : value;
}

double inverse(double transformed, Scale scale)
{
    return scale == Scale::LOG ? std::pow(10.0, transformed) : transformed;
}

}  // namespace

std::optional<double> interpolateY(const SeriesData& data, double x, Scale xScale, Scale yScale)
{
    if (!data.isSortedByX() || data.empty() || !std::isfinite(x) || x < data.x(0) ||
        x > data.x(data.size() - 1))
    {
        return std::nullopt;
    }
    const std::size_t after = data.lowerBound(x);  // the first point at or after x
    if (data.x(after) == x)
    {
        const double y = data.y(after);
        return forward(y, yScale) ? std::optional(y) : std::nullopt;
    }
    const std::size_t           before = after - 1;  // x is within the range: after > 0
    const std::optional<double> x0     = forward(data.x(before), xScale);
    const std::optional<double> x1     = forward(data.x(after), xScale);
    const std::optional<double> at     = forward(x, xScale);
    const std::optional<double> y0     = forward(data.y(before), yScale);
    const std::optional<double> y1     = forward(data.y(after), yScale);
    if (!x0 || !x1 || !at || !y0 || !y1)
    {
        return std::nullopt;
    }
    const double t = (*at - *x0) / (*x1 - *x0);
    return inverse(*y0 + (t * (*y1 - *y0)), yScale);
}

}  // namespace rocketplot::core
