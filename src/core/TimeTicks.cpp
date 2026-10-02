#include "core/TimeTicks.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/NumberFormatter.h"
#include "core/TickGenerator.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

constexpr double kMillisecond   = 0.001;
constexpr double kMinute        = 60.0;
constexpr double kHour          = 3600.0;
constexpr double kDay           = 86400.0;
constexpr double kMonth         = 30.436875 * kDay;  // average Gregorian month
constexpr double kYear          = 365.2425 * kDay;   // average Gregorian year
constexpr int    kMonthsPerYear = 12;

constexpr std::array<std::string_view, 12> kMonthNames = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

enum class Unit : std::uint8_t
{
    SUBMILLISECOND,
    MILLISECOND,
    SECOND,
    MINUTE,
    HOUR,
    DAY,
    MONTH,
    YEAR,
};

// A tick interval: count units, with minorDivisions minor intervals per step (0: none).
struct Step
{
    Unit unit;
    int  count;
    int  minorDivisions;
};

constexpr auto kSteps = std::to_array<Step>({
    {.unit = Unit::MILLISECOND, .count = 1, .minorDivisions = 5},
    {.unit = Unit::MILLISECOND, .count = 2, .minorDivisions = 4},
    {.unit = Unit::MILLISECOND, .count = 5, .minorDivisions = 5},
    {.unit = Unit::MILLISECOND, .count = 10, .minorDivisions = 5},
    {.unit = Unit::MILLISECOND, .count = 20, .minorDivisions = 4},
    {.unit = Unit::MILLISECOND, .count = 50, .minorDivisions = 5},
    {.unit = Unit::MILLISECOND, .count = 100, .minorDivisions = 5},
    {.unit = Unit::MILLISECOND, .count = 200, .minorDivisions = 4},
    {.unit = Unit::MILLISECOND, .count = 500, .minorDivisions = 5},
    {.unit = Unit::SECOND, .count = 1, .minorDivisions = 5},
    {.unit = Unit::SECOND, .count = 2, .minorDivisions = 4},
    {.unit = Unit::SECOND, .count = 5, .minorDivisions = 5},
    {.unit = Unit::SECOND, .count = 10, .minorDivisions = 5},
    {.unit = Unit::SECOND, .count = 15, .minorDivisions = 3},
    {.unit = Unit::SECOND, .count = 30, .minorDivisions = 6},
    {.unit = Unit::MINUTE, .count = 1, .minorDivisions = 6},
    {.unit = Unit::MINUTE, .count = 2, .minorDivisions = 4},
    {.unit = Unit::MINUTE, .count = 5, .minorDivisions = 5},
    {.unit = Unit::MINUTE, .count = 10, .minorDivisions = 5},
    {.unit = Unit::MINUTE, .count = 15, .minorDivisions = 3},
    {.unit = Unit::MINUTE, .count = 30, .minorDivisions = 6},
    {.unit = Unit::HOUR, .count = 1, .minorDivisions = 6},
    {.unit = Unit::HOUR, .count = 2, .minorDivisions = 4},
    {.unit = Unit::HOUR, .count = 3, .minorDivisions = 3},
    {.unit = Unit::HOUR, .count = 6, .minorDivisions = 6},
    {.unit = Unit::HOUR, .count = 12, .minorDivisions = 6},
    {.unit = Unit::DAY, .count = 1, .minorDivisions = 4},
    {.unit = Unit::DAY, .count = 2, .minorDivisions = 2},
    {.unit = Unit::DAY, .count = 7, .minorDivisions = 7},
    {.unit = Unit::DAY, .count = 14, .minorDivisions = 14},
    {.unit = Unit::MONTH, .count = 1, .minorDivisions = 0},
    {.unit = Unit::MONTH, .count = 2, .minorDivisions = 2},
    {.unit = Unit::MONTH, .count = 3, .minorDivisions = 3},
    {.unit = Unit::MONTH, .count = 6, .minorDivisions = 6},
    {.unit = Unit::YEAR, .count = 1, .minorDivisions = 4},
    {.unit = Unit::YEAR, .count = 2, .minorDivisions = 2},
    {.unit = Unit::YEAR, .count = 5, .minorDivisions = 5},
    {.unit = Unit::YEAR, .count = 10, .minorDivisions = 5},
    {.unit = Unit::YEAR, .count = 20, .minorDivisions = 4},
    {.unit = Unit::YEAR, .count = 50, .minorDivisions = 5},
    {.unit = Unit::YEAR, .count = 100, .minorDivisions = 5},
    {.unit = Unit::YEAR, .count = 200, .minorDivisions = 4},
    {.unit = Unit::YEAR, .count = 500, .minorDivisions = 5},
    {.unit = Unit::YEAR, .count = 1000, .minorDivisions = 5},
});

double unitSeconds(Unit unit)
{
    switch (unit)
    {
        case Unit::SUBMILLISECOND:
        case Unit::MILLISECOND:
            return kMillisecond;
        case Unit::SECOND:
            return 1.0;
        case Unit::MINUTE:
            return kMinute;
        case Unit::HOUR:
            return kHour;
        case Unit::DAY:
            return kDay;
        case Unit::MONTH:
            return kMonth;
        case Unit::YEAR:
            return kYear;
    }
    return 1.0;
}

double stepSeconds(const Step& step)
{
    return step.count * unitSeconds(step.unit);
}

// A local time split into calendar fields; second includes the fraction.
struct Civil
{
    int    year   = 1970;
    int    month  = 1;
    int    day    = 1;
    int    hour   = 0;
    int    minute = 0;
    double second = 0.0;
};

double dayNumber(double local)
{
    return std::floor(local / kDay);
}

double localDay(int year, int month, int day)
{
    const std::chrono::sys_days date{std::chrono::year{year} / month / day};
    return static_cast<double>(date.time_since_epoch().count()) * kDay;
}

Civil civilOf(double local)
{
    const double                      days = dayNumber(local);
    const std::chrono::year_month_day date{
        std::chrono::sys_days{std::chrono::days{static_cast<std::chrono::days::rep>(days)}},
    };
    const double secondOfDay = local - (days * kDay);
    Civil        civil;
    civil.year   = static_cast<int>(date.year());
    civil.month  = static_cast<int>(static_cast<unsigned>(date.month()));
    civil.day    = static_cast<int>(static_cast<unsigned>(date.day()));
    civil.hour   = static_cast<int>(secondOfDay / kHour);
    civil.minute = static_cast<int>((secondOfDay - (civil.hour * kHour)) / kMinute);
    civil.second = secondOfDay - (civil.hour * kHour) - (civil.minute * kMinute);
    return civil;
}

// Converts with the time zone's offset at that time; local to UTC needs the offset at the result,
// found by one correction (exact except in the hour a daylight saving change skips or repeats).
class Clock
{
public:
    explicit Clock(const UtcOffset& offset) : m_offset(&offset) { }

    [[nodiscard]] double toLocal(double utc) const { return utc + offsetAt(utc); }
    [[nodiscard]] double toUtc(double local) const
    {
        return local - offsetAt(local - offsetAt(local));
    }

private:
    [[nodiscard]] double offsetAt(double utc) const { return *m_offset ? (*m_offset)(utc) : 0.0; }

    const UtcOffset* m_offset;
};

std::string twoDigits(int value)
{
    return std::format("{:02}", value);
}

std::string dayLabel(const Civil& civil)
{
    return std::format("{} {}", kMonthNames.at(static_cast<std::size_t>(civil.month - 1)),
                       civil.day);
}

// The label of a major tick at local time `local`, for ticks of `unit` (decimals: digits of the
// second, below a second).
std::string labelFor(double local, Unit unit, int decimals)
{
    const double scale       = std::pow(10.0, decimals);
    const double quantized   = std::round(local * scale) / scale;  // 14:59:59.9999 is 15:00:00.000
    const Civil  civil       = civilOf(quantized);
    const bool   wholeMinute = civil.second < 0.5 / scale;
    const bool   midnight    = civil.hour == 0 && civil.minute == 0 && wholeMinute;
    const bool   newYear     = midnight && civil.month == 1 && civil.day == 1;
    std::string  year        = std::format("{}", civil.year);
    std::string  date        = newYear ? year : dayLabel(civil);
    switch (unit)
    {
        case Unit::YEAR:
            return year;
        case Unit::MONTH:
            return civil.month == 1
                       ? year
                       : std::string(kMonthNames.at(static_cast<std::size_t>(civil.month - 1)));
        case Unit::DAY:
            return date;
        case Unit::HOUR:
        case Unit::MINUTE:
            return midnight ? date : twoDigits(civil.hour) + ":" + twoDigits(civil.minute);
        case Unit::SECOND:
            return midnight ? date
                            : std::format("{:02}:{:02}:{:02}", civil.hour, civil.minute,
                                          static_cast<int>(std::lround(civil.second)));
        case Unit::MILLISECOND:
        case Unit::SUBMILLISECOND:
            if (wholeMinute)
            {
                return midnight ? date : twoDigits(civil.hour) + ":" + twoDigits(civil.minute);
            }
            return std::format("{:0{}.{}f}", civil.second, decimals + 3, decimals);
    }
    return {};
}

std::string contextFor(double local, Unit unit)
{
    const Civil civil = civilOf(local);
    switch (unit)
    {
        case Unit::YEAR:
            return {};
        case Unit::MONTH:
        case Unit::DAY:
            return std::format("{}", civil.year);
        case Unit::HOUR:
        case Unit::MINUTE:
        case Unit::SECOND:
            return std::format("{}-{:02}-{:02}", civil.year, civil.month, civil.day);
        case Unit::MILLISECOND:
        case Unit::SUBMILLISECOND:
            return std::format("{}-{:02}-{:02} {:02}:{:02}", civil.year, civil.month, civil.day,
                               civil.hour, civil.minute);
    }
    return {};
}

// Local tick times for a step whose ticks fall on calendar days, months or years.
struct CalendarTicks
{
    std::vector<double> major;
    std::vector<double> minor;
};

// The last day of the month a step of @p count days starts on: steps of 2, 7 and 14 days restart
// every month (1, 3, 5 ... 29; 1, 8, 15, 22; 1, 15).
int lastStartDay(int count)
{
    switch (count)
    {
        case 2:
            return 29;
        case 7:
            return 22;
        default:
            return 15;
    }
}

CalendarTicks dayTicks(Range local, int count, bool withMinor)
{
    const int     lastDay = lastStartDay(count);
    CalendarTicks ticks;
    const auto    first = static_cast<std::int64_t>(dayNumber(local.min));
    const auto    last  = static_cast<std::int64_t>(dayNumber(local.max));
    for (std::int64_t day = first; day <= last; ++day)
    {
        const double time = static_cast<double>(day) * kDay;
        if (time < local.min)
        {
            continue;
        }
        const Civil civil = civilOf(time);
        if ((civil.day - 1) % count == 0 && civil.day <= lastDay)
        {
            ticks.major.push_back(time);
        }
        else if (withMinor)
        {
            ticks.minor.push_back(time);
        }
    }
    return ticks;
}

CalendarTicks monthTicks(Range local, int count, bool withMinor)
{
    CalendarTicks ticks;
    const Civil   start = civilOf(local.min);
    int           year  = start.year;
    int           month = start.month;
    while (true)
    {
        const double time = localDay(year, month, 1);
        if (time > local.max)
        {
            break;
        }
        if (time >= local.min)
        {
            if ((month - 1) % count == 0)
            {
                ticks.major.push_back(time);
            }
            else if (withMinor)
            {
                ticks.minor.push_back(time);
            }
        }
        if (++month > kMonthsPerYear)
        {
            month = 1;
            ++year;
        }
    }
    return ticks;
}

CalendarTicks yearTicks(Range local, int count, int minorDivisions, bool withMinor)
{
    CalendarTicks ticks;
    const int     first      = civilOf(local.min).year;
    const int     last       = civilOf(local.max).year;
    const auto    multipleOf = [](int value, int of) { return ((value % of) + of) % of == 0; };
    for (int year = first; year <= last; ++year)
    {
        const double time = localDay(year, 1, 1);
        if (time >= local.min && multipleOf(year, count))
        {
            ticks.major.push_back(time);
        }
        else if (time >= local.min && withMinor && count > 1 &&
                 multipleOf(year, count / minorDivisions))
        {
            ticks.minor.push_back(time);
        }
        // Yearly ticks get quarters as minor ticks.
        for (int month = 4; withMinor && count == 1 && month <= kMonthsPerYear; month += 3)
        {
            const double quarter = localDay(year, month, 1);
            if (quarter >= local.min && quarter <= local.max)
            {
                ticks.minor.push_back(quarter);
            }
        }
    }
    return ticks;
}

// The finest step at least @p target seconds long.
const Step& stepFor(double target)
{
    for (const Step& step : kSteps)
    {
        if (stepSeconds(step) >= target)
        {
            return step;
        }
    }
    return kSteps.back();
}

// Ticks in local time, before conversion to UTC, with how to label them.
struct LocalTicks
{
    std::vector<double> major;
    std::vector<double> minor;
    Unit                unit     = Unit::SUBMILLISECOND;
    int                 decimals = 0;  // digits of the second, below a second
};

LocalTicks localTicks(Range local, double span, double lengthPx, double minSpacingPx,
                      double minMinorSpacingPx)
{
    LocalTicks   result;
    const double target    = span / std::max(1.0, std::floor(lengthPx / minSpacingPx));
    const auto   minorFits = [&](double seconds) {
        return seconds / span * lengthPx >= minMinorSpacingPx;
    };
    if (target < kMillisecond)
    {
        Ticks ticks     = linearTicks(local, lengthPx, minSpacingPx, minMinorSpacingPx);
        result.major    = std::move(ticks.major);
        result.minor    = std::move(ticks.minor);
        result.decimals = decimalsForStep(ticks.step);
        return result;
    }
    const Step& step = stepFor(target);
    result.unit      = step.unit;
    const bool withMinor =
        step.minorDivisions > 0 && minorFits(stepSeconds(step) / step.minorDivisions);
    if (step.unit <= Unit::HOUR || (step.unit == Unit::DAY && step.count == 1))
    {
        // Fixed-length steps: multiples of the step in local seconds (local days are 86400 s).
        const double seconds = stepSeconds(step);
        result.major         = multiplesOf(seconds, local);
        if (withMinor)
        {
            result.minor = minorMultiples(seconds, step.minorDivisions, local);
        }
        result.decimals = step.unit == Unit::MILLISECOND ? decimalsForStep(seconds) : 0;
        return result;
    }
    CalendarTicks ticks;
    if (step.unit == Unit::DAY)
    {
        ticks = dayTicks(local, step.count, minorFits(kDay));
    }
    else if (step.unit == Unit::MONTH)
    {
        ticks = monthTicks(local, step.count, withMinor);
    }
    else
    {
        ticks = yearTicks(local, step.count, step.minorDivisions, withMinor);
    }
    result.major = std::move(ticks.major);
    result.minor = std::move(ticks.minor);
    std::ranges::sort(result.minor);
    return result;
}

}  // namespace

TimeTicks timeTicks(Range utcRange, double lengthPx, double minSpacingPx, double minMinorSpacingPx,
                    const UtcOffset& utcOffset)
{
    TimeTicks result;
    if (!utcRange.isValid() || !(utcRange.span() > 0.0) || !(lengthPx > 0.0) ||
        !(minSpacingPx > 0.0))
    {
        return result;
    }
    const Clock      clock(utcOffset);
    const Range      local{.min = clock.toLocal(utcRange.min), .max = clock.toLocal(utcRange.max)};
    const LocalTicks ticks =
        localTicks(local, utcRange.span(), lengthPx, minSpacingPx, minMinorSpacingPx);

    // Back to UTC. Local times a daylight saving change skips don't exist: they don't survive the
    // round trip, and are dropped.
    const auto exists = [&](double time, double utc) {
        return std::abs(clock.toLocal(utc) - time) < 1e-3;
    };
    for (const double time : ticks.major)
    {
        const double utc = clock.toUtc(time);
        if (exists(time, utc) && (result.major.empty() || utc > result.major.back()))
        {
            result.major.push_back(utc);
            result.labels.push_back(labelFor(time, ticks.unit, ticks.decimals));
        }
    }
    for (const double time : ticks.minor)
    {
        const double utc = clock.toUtc(time);
        if (exists(time, utc))
        {
            result.minor.push_back(utc);
        }
    }
    result.context = contextFor(local.min, ticks.unit);
    return result;
}

double localSeconds(int year, int month, int day, int hour, int minute, double second)
{
    return localDay(year, month, day) + (hour * kHour) + (minute * kMinute) + second;
}

std::string formatDateTime(double localSeconds, int decimals)
{
    const Civil civil = civilOf(localSeconds);
    return std::format("{}-{:02}-{:02} {:02}:{:02}:{:0{}.{}f}", civil.year, civil.month, civil.day,
                       civil.hour, civil.minute, civil.second, decimals > 0 ? decimals + 3 : 2,
                       std::max(0, decimals));
}

}  // namespace rocketplot::core
