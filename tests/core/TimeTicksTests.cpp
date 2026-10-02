#include "core/TimeTicks.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::formatDateTime;
using rocketplot::core::formatTime;
using rocketplot::core::localSeconds;
using rocketplot::core::timeTicks;
using rocketplot::core::UtcOffset;

constexpr double kHour = 3600.0;
constexpr double kDay  = 86400.0;

// 2026-03-28 00:00 UTC
constexpr double kStart = 1774656000.0;

bool contains(const std::vector<std::string>& labels, const std::string& label)
{
    return std::ranges::find(labels, label) != labels.end();
}

TEST(TimeTicks, CalendarArithmetic)
{
    EXPECT_DOUBLE_EQ(localSeconds(1970, 1, 1), 0.0);
    EXPECT_DOUBLE_EQ(localSeconds(2000, 3, 1) - localSeconds(2000, 2, 28), 2 * kDay);  // leap year
    EXPECT_DOUBLE_EQ(localSeconds(1900, 3, 1) - localSeconds(1900, 2, 28),
                     kDay);  // not a leap year
    EXPECT_EQ(formatDateTime(localSeconds(2026, 3, 28, 14, 30, 15.25), 3),
              "2026-03-28 14:30:15.250");
    EXPECT_EQ(formatDateTime(localSeconds(1969, 12, 31, 23, 59, 59)), "1969-12-31 23:59:59");
}

TEST(TimeTicks, HoursWithTheDateAtMidnight)
{
    // 36 hours over 900 px, ticks at least 80 px apart: every 6 hours.
    const auto ticks = timeTicks(Range{.min = kStart - (6 * kHour), .max = kStart + (30 * kHour)},
                                 900.0, 80.0, 4.0);
    ASSERT_GE(ticks.major.size(), 6U);
    EXPECT_DOUBLE_EQ(ticks.major[1] - ticks.major[0], 6 * kHour);
    EXPECT_TRUE(contains(ticks.labels, "Mar 28"));  // midnight shows the day
    EXPECT_TRUE(contains(ticks.labels, "06:00"));
    EXPECT_TRUE(contains(ticks.labels, "Mar 29"));
    EXPECT_EQ(ticks.context, "2026-03-27");  // the date at the start of the range
    EXPECT_FALSE(ticks.minor.empty());
}

TEST(TimeTicks, MillisecondsShowSecondsOfTheMinute)
{
    const double t     = localSeconds(2026, 3, 28, 14, 30, 15.0);
    const auto   ticks = timeTicks(Range{.min = t, .max = t + 1.0}, 1000.0, 90.0, 4.0);
    ASSERT_GE(ticks.major.size(), 6U);
    EXPECT_TRUE(contains(ticks.labels, "15.2"));  // 100 ms apart: one decimal
    EXPECT_TRUE(contains(ticks.labels, "15.4"));
    EXPECT_EQ(ticks.context, "2026-03-28 14:30");
}

TEST(TimeTicks, MinuteBoundaryInMilliseconds)
{
    const double t     = localSeconds(2026, 3, 28, 14, 30, 59.0);
    const auto   ticks = timeTicks(Range{.min = t, .max = t + 2.0}, 1000.0, 90.0, 4.0);
    EXPECT_TRUE(contains(ticks.labels, "14:31"));
}

TEST(TimeTicks, SecondsLabels)
{
    const double t     = localSeconds(2026, 3, 28, 14, 30, 0.0);
    const auto   ticks = timeTicks(Range{.min = t, .max = t + 60.0}, 800.0, 80.0, 4.0);
    EXPECT_TRUE(contains(ticks.labels, "14:30:10"));
}

TEST(TimeTicks, DaysMonthsAndYears)
{
    const auto days =
        timeTicks(Range{.min = kStart, .max = kStart + (10 * kDay)}, 1000.0, 80.0, 4.0);
    EXPECT_TRUE(contains(days.labels, "Mar 30"));
    EXPECT_TRUE(contains(days.labels, "Apr 1"));
    EXPECT_EQ(days.context, "2026");

    const auto months =
        timeTicks(Range{.min = localSeconds(2025, 6, 15), .max = localSeconds(2026, 9, 1)}, 1000.0,
                  60.0, 4.0);
    EXPECT_TRUE(contains(months.labels, "Aug"));
    EXPECT_TRUE(contains(months.labels, "2026"));  // January shows the year

    const auto years = timeTicks(
        Range{.min = localSeconds(1990, 1, 1), .max = localSeconds(2030, 1, 1)}, 1000.0, 80.0, 4.0);
    EXPECT_TRUE(contains(years.labels, "2000"));
    EXPECT_EQ(years.context, "");
    for (const double tick : years.major)
    {
        // Every tick is on January 1st.
        EXPECT_EQ(formatDateTime(tick).substr(4, 6), "-01-01") << formatDateTime(tick);
    }
}

TEST(TimeTicks, WeeksStartOnTheSameDaysEachMonth)
{
    const auto ticks =
        timeTicks(Range{.min = localSeconds(2026, 3, 1), .max = localSeconds(2026, 5, 31)}, 1000.0,
                  80.0, 4.0);
    for (const std::string& label : ticks.labels)
    {
        const std::string day = label.substr(label.find(' ') + 1);
        EXPECT_TRUE(day == "1" || day == "8" || day == "15" || day == "22") << label;
    }
}

TEST(TimeTicks, TimeZoneOffsetsMoveTicksToLocalBoundaries)
{
    // UTC+2: local midnight is 22:00 UTC the day before.
    const UtcOffset plusTwo = [](double) { return 2 * kHour; };
    const auto      ticks =
        timeTicks(Range{.min = kStart, .max = kStart + kDay}, 900.0, 80.0, 4.0, plusTwo);
    EXPECT_TRUE(contains(ticks.labels, "Mar 29"));
    const auto midnight = std::ranges::find(ticks.labels, "Mar 29") - ticks.labels.begin();
    EXPECT_DOUBLE_EQ(ticks.major[static_cast<std::size_t>(midnight)], kStart + kDay - (2 * kHour));
}

TEST(TimeTicks, DaylightSavingChangeKeepsLabelsOnLocalHours)
{
    // A zone that moves from UTC+1 to UTC+2 at 2026-03-29 01:00 UTC (as Central Europe does).
    const double    change = localSeconds(2026, 3, 29, 1);
    const UtcOffset europe = [change](double utc) { return utc < change ? kHour : 2 * kHour; };
    const auto ticks = timeTicks(Range{.min = change - (5 * kHour), .max = change + (5 * kHour)},
                                 1200.0, 60.0, 4.0, europe);
    // Hourly ticks; local 02:00 doesn't exist that night.
    EXPECT_TRUE(contains(ticks.labels, "01:00"));
    EXPECT_TRUE(contains(ticks.labels, "03:00"));
    EXPECT_FALSE(contains(ticks.labels, "02:00"));
    EXPECT_TRUE(std::ranges::is_sorted(ticks.major));
}

TEST(TimeTicks, SubMillisecond)
{
    const double t     = localSeconds(2026, 3, 28, 14, 30, 15.0);
    const auto   ticks = timeTicks(Range{.min = t, .max = t + 0.002}, 1000.0, 90.0, 4.0);
    ASSERT_GE(ticks.major.size(), 2U);
    EXPECT_TRUE(contains(ticks.labels, "15.0010") || contains(ticks.labels, "15.0005"));
}

TEST(TimeTicks, DegenerateInputs)
{
    EXPECT_TRUE(timeTicks(Range{.min = kStart, .max = kStart}, 900.0, 80.0, 4.0).major.empty());
    EXPECT_TRUE(timeTicks(Range::empty(), 900.0, 80.0, 4.0).major.empty());
}

// 2026-03-28 14:30:15.256 UTC
constexpr double kReadoutTime = kStart + (14 * kHour) + (30 * 60) + 15.256;

TEST(TimeTicks, ReadoutsOfDaysAndMinutes)
{
    EXPECT_EQ(formatTime(kReadoutTime, kDay), "2026-03-28");
    EXPECT_EQ(formatTime(kReadoutTime + (9 * kHour), kDay), "2026-03-28");  // 23:30: that day
    EXPECT_EQ(formatTime(kReadoutTime, kHour), "2026-03-28 14:30");
    EXPECT_EQ(formatTime(kReadoutTime, 60.0), "2026-03-28 14:30");
    EXPECT_EQ(formatTime(kReadoutTime + 30.0, 60.0), "2026-03-28 14:31");
}

TEST(TimeTicks, ReadoutsOfSeconds)
{
    EXPECT_EQ(formatTime(kReadoutTime, 1.0), "2026-03-28 14:30:15");
    EXPECT_EQ(formatTime(kReadoutTime, 0.03), "2026-03-28 14:30:15.26");
    EXPECT_EQ(formatTime(kReadoutTime, 0.001), "2026-03-28 14:30:15.256");
    // In a time zone two hours ahead.
    const UtcOffset plusTwo = [](double) { return 2 * kHour; };
    EXPECT_EQ(formatTime(kReadoutTime, 1.0, plusTwo), "2026-03-28 16:30:15");
    EXPECT_EQ(formatTime(std::numeric_limits<double>::quiet_NaN(), 1.0), "");
}

}  // namespace
