#pragma once

#include <QDateTime>
#include <chrono>

#include "rocketplot/export.h"

/// Conversions to and from the values of a DATE_TIME axis: seconds since 1970-01-01 00:00 UTC, as a
/// double (microsecond precision for current dates).
namespace rocketplot
{

/// The axis value of @p time.
[[nodiscard]] ROCKETPLOT_EXPORT double toPlotTime(const QDateTime& time);

/// The axis value of a std::chrono::system_clock time.
template <class Duration>
[[nodiscard]] double toPlotTime(std::chrono::sys_time<Duration> time)
{
    return std::chrono::duration<double>(time.time_since_epoch()).count();
}

/// The UTC time of an axis value.
[[nodiscard]] ROCKETPLOT_EXPORT QDateTime fromPlotTime(double seconds);

}  // namespace rocketplot
