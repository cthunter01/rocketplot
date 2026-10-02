#pragma once

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// The range an autoscaled axis shows for data within @p bounds: widened by @p margin (a fraction
/// of the span) on each side. A single value gets a range around it, and no data at all gets [0,
/// 1].
[[nodiscard]] Range autoscaleRange(Range bounds, double margin);

}  // namespace rocketplot::core
