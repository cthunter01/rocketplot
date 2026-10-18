#include "core/AxisTicks.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "core/NumberFormatter.h"
#include "core/TickGenerator.h"
#include "core/TimeTicks.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// The category at @p value, if there is one there: the nearest whole index.
std::optional<std::size_t> categoryAt(double value, std::span<const std::string> categories)
{
    const double index = std::round(value);
    if (!(index >= 0.0) || !(index < static_cast<double>(categories.size())))  // NaN too
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(index);
}

AxisTicks categoryTicks(const TickRequest& request)
{
    AxisTicks    result;
    const Range  range = request.range;
    const double first = std::max(0.0, std::ceil(range.min));
    const double last =
        std::min(static_cast<double>(request.categories.size()) - 1.0, std::floor(range.max));
    if (!(first <= last))  // none in the range, or NaN
    {
        return result;
    }
    const double perCategory = range.span() > 0.0 ? request.lengthPx / range.span() : 0.0;
    std::size_t  step        = 1;
    if (perCategory > 0.0 && perCategory < request.minSpacingPx)
    {
        step = static_cast<std::size_t>(std::ceil(request.minSpacingPx / perCategory));
    }
    // Every step-th category from the first, so that the same ones keep their ticks as the view
    // pans.
    const auto from = static_cast<std::size_t>(first);
    const auto to   = static_cast<std::size_t>(last);
    for (std::size_t index = (from + step - 1) / step * step; index <= to; index += step)
    {
        result.major.push_back(static_cast<double>(index));
        result.labels.push_back(request.categories[index]);
    }
    return result;
}

}  // namespace

AxisTicks makeTicks(const TickRequest& request)
{
    if (request.kind == TickKind::CATEGORY)
    {
        return categoryTicks(request);
    }
    AxisTicks result;
    if (request.kind == TickKind::TIME)
    {
        TimeTicks ticks   = timeTicks(request.range, request.lengthPx, request.minSpacingPx,
                                      request.minMinorSpacingPx, request.utcOffset);
        result.major      = std::move(ticks.major);
        result.minor      = std::move(ticks.minor);
        result.labels     = std::move(ticks.labels);
        result.annotation = ticks.context.empty() || request.zoneName.empty()
                                ? std::move(ticks.context)
                                : ticks.context + " " + request.zoneName;
        return result;
    }
    if (request.kind == TickKind::LOG && fitsDecadeTicks(request.range))
    {
        Ticks ticks  = logTicks(request.range, request.lengthPx, request.minSpacingPx,
                                request.minMinorSpacingPx);
        result.major = std::move(ticks.major);
        result.minor = std::move(ticks.minor);
        for (const double value : result.major)
        {
            result.labels.push_back(formatLogLabel(value, request.style));
        }
        return result;
    }
    Ticks          ticks    = linearTicks(request.range, request.lengthPx, request.minSpacingPx,
                                          request.minMinorSpacingPx);
    const Labeling labeling = chooseLabeling(request.range, ticks.step, request.style);
    result.major            = std::move(ticks.major);
    result.minor            = std::move(ticks.minor);
    for (const double value : result.major)
    {
        result.labels.push_back(formatLabel(value, labeling));
    }
    result.annotation = labelingAnnotation(labeling);
    return result;
}

std::string formatReadout(double value, double resolution, TickKind kind, NumberStyle style,
                          const UtcOffset& utcOffset, std::span<const std::string> categories)
{
    if (kind == TickKind::TIME)
    {
        return formatTime(value, resolution, utcOffset);
    }
    if (kind == TickKind::CATEGORY)
    {
        if (const std::optional<std::size_t> index = categoryAt(value, categories))
        {
            return categories[*index];
        }
    }
    return formatValue(value, resolution, style);
}

}  // namespace rocketplot::core
