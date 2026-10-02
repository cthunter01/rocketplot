#pragma once

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// The range an autoscaled axis shows for data within @p bounds: widened by @p margin (a fraction
/// of the span) on each side; on a log scale, in log space (pass the bounds of the positive
/// values). A single value gets a range around it, and no data at all gets [0, 1] ([1, 10] on a log
/// scale).
[[nodiscard]] Range autoscaleRange(Range bounds, double margin, Scale scale = Scale::LINEAR);

/// The range of an axis following the newest data: the last @p window of @p bounds, with a
/// margin's share of the window left free after the newest value. While the data spans less than
/// the window, the window starts at the oldest value instead.
[[nodiscard]] Range followRange(Range bounds, double window, double margin);

}  // namespace rocketplot::core
