#include "core/AxisTicks.h"

#include <string>
#include <utility>

#include "core/NumberFormatter.h"
#include "core/TickGenerator.h"
#include "core/TimeTicks.h"

namespace rocketplot::core
{

AxisTicks makeTicks(const TickRequest& request)
{
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

}  // namespace rocketplot::core
