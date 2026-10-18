#pragma once

#include <cstddef>

#include "core/AxisMapping.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// When an autoscaled range reaches zero, on a linear scale.
struct ZeroRule
{
    /// Data that stops short of 0 by at most this share of its span is shown down (or up) to 0:
    /// [5, 100] reaches 0 from 5 / 95 on. 0 never reaches it; infinity always does.
    double includeWithin = 0.0;
    /// An end of the data that is exactly 0 stays at 0 (bars that start there), with no margin
    /// beyond it.
    bool baseline = false;
};

/// The range an autoscaled axis shows for data within @p bounds: widened by @p margin (a fraction
/// of the span) on each side; on a log scale, in log space (pass the bounds of the positive
/// values). A single value gets a range around it, and no data at all gets [0, 1] ([1, 10] on a log
/// scale). Where @p zero takes the range to 0, that end is exactly 0 and the margin goes on the
/// other end only: [5, 100] with a margin of 0.03 and 0.3 for ZeroRule::includeWithin is [0, 103].
/// A log scale has no zero, and ignores @p zero.
[[nodiscard]] Range autoscaleRange(Range bounds, double margin, Scale scale = Scale::LINEAR,
                                   ZeroRule zero = {});

/// What alignZeros() makes of two ranges.
struct AlignedZeros
{
    Range first;
    Range second;
};

/// Two linear ranges that both hold 0, widened so that 0 is as far up the one as up the other (a
/// left and a right axis whose zeros line up): each at one end at most, by the least growth in all
/// (as shares of their spans). [0, x] and [-y, 0] are widened to [-x, x] and [-y, y]. Ranges that
/// don't both hold 0, or that are too small to tell, are returned as they are.
[[nodiscard]] AlignedZeros alignZeros(Range first, Range second);

/// The range a category axis of @p count categories shows: each category (at 0, 1, ...) with half
/// a unit either side, joined with @p bounds of the data on it. No margin and no zero rule: the
/// categories are what is shown. With no categories, as if there were one: [-0.5, 0.5].
[[nodiscard]] Range categoryRange(Range bounds, std::size_t count);

/// The range of an axis following the newest data: the last @p window of @p bounds, with a
/// margin's share of the window left free after the newest value. While the data spans less than
/// the window, the window starts at the oldest value instead.
[[nodiscard]] Range followRange(Range bounds, double window, double margin);

/// @p range made taller at the top so that its top @p share is left free (room for what is drawn
/// over the data there, such as event flags): @p range takes the bottom 1 - share of the result.
/// On a log scale, in log space. A share outside [0, 1) leaves @p range as it is.
[[nodiscard]] Range withHeadroom(Range range, double share, Scale scale = Scale::LINEAR);

}  // namespace rocketplot::core
