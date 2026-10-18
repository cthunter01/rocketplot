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

/// @p range made taller at the top so that its top @p share is left free (room for what is drawn
/// over the data there, such as event flags): @p range takes the bottom 1 - share of the result.
/// On a log scale, in log space. A share outside [0, 1) leaves @p range as it is.
[[nodiscard]] Range withHeadroom(Range range, double share, Scale scale = Scale::LINEAR);

}  // namespace rocketplot::core
