#pragma once

#include <functional>
#include <string>
#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// Seconds to add to a UTC time (seconds since the Unix epoch) for local time at that instant: the
/// time zone's offset, which changes with daylight saving time. Empty means UTC.
using UtcOffset = std::function<double(double utcSeconds)>;

/// Ticks and labels for a date/time axis.
struct TimeTicks
{
    std::vector<double>      major;  ///< UTC seconds
    std::vector<double>      minor;
    std::vector<std::string> labels;  ///< One per major tick
    /// The part of the date and time the labels leave out, for the start of the range: "2026-03-28"
    /// when labels are times of day, "2026" when they are days, "" for years.
    std::string context;
};

/// Ticks on calendar boundaries in local time (at least @p minSpacingPx apart over @p lengthPx
/// pixels): 1, 2, 5, 10 ... 500 ms; 1, 2, 5, 10, 15, 30 s; 1, 2, 5, 10, 15, 30 min; 1, 2, 3, 6, 12
/// h; days (1st, 8th, 15th, 22nd for weeks); 1, 2, 3, 6 months; 1, 2, 5, 10 ... years. Below a
/// millisecond, ticks are plain multiples of a nice number of seconds.
///
/// Labels show what changes between ticks, concisely: "14:30", "14:30:15", "15.250" (seconds of
/// the minute), "Mar 5", "Mar", "2026". At a boundary of the next larger unit they show that
/// instead, so a day starts with "Mar 6" rather than "00:00", a minute with "14:31" and a year
/// with "2026".
[[nodiscard]] TimeTicks timeTicks(Range utcRange, double lengthPx, double minSpacingPx,
                                  double minMinorSpacingPx, const UtcOffset& utcOffset = {});

/// Local seconds since 1970-01-01 00:00 for a calendar date and time (proleptic Gregorian).
[[nodiscard]] double localSeconds(int year, int month, int day, int hour = 0, int minute = 0,
                                  double second = 0.0);

/// A time on its own, such as a crosshair's readout, written to @p resolution seconds (e.g. the
/// time one pixel spans) in local time: "2026-03-28" for half a day or more, then "2026-03-28
/// 14:30", "2026-03-28 14:30:15" and "2026-03-28 14:30:15.25" (up to microseconds).
[[nodiscard]] std::string formatTime(double utcSeconds, double resolution,
                                     const UtcOffset& utcOffset = {});

/// "2026-03-28 14:30:15.250" for local seconds, with @p decimals digits of the second.
[[nodiscard]] std::string formatDateTime(double localSeconds, int decimals = 0);

}  // namespace rocketplot::core
