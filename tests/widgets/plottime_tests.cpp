#include "rocketplot/plottime.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>
#include <chrono>

#include <gtest/gtest.h>

namespace
{

TEST(PlotTime, QDateTimeRoundTrip)
{
    const QDateTime time(QDate(2026, 3, 28), QTime(14, 30, 15, 250), QTimeZone::utc());
    const double    seconds = rocketplot::toPlotTime(time);
    EXPECT_DOUBLE_EQ(seconds, 1774708215.25);
    EXPECT_EQ(rocketplot::fromPlotTime(seconds), time);
}

TEST(PlotTime, LocalTimesAreConvertedToUtc)
{
    const QDateTime berlin(QDate(2026, 7, 1), QTime(12, 0),
                           QTimeZone(QByteArrayLiteral("Europe/Berlin")));
    const QDateTime utc(QDate(2026, 7, 1), QTime(10, 0), QTimeZone::utc());
    EXPECT_DOUBLE_EQ(rocketplot::toPlotTime(berlin), rocketplot::toPlotTime(utc));
}

TEST(PlotTime, Chrono)
{
    using namespace std::chrono;
    const sys_time<milliseconds> time = sys_days{year{2026} / March / day{28}} + hours{14} +
                                        minutes{30} + seconds{15} + milliseconds{250};
    EXPECT_DOUBLE_EQ(rocketplot::toPlotTime(time), 1774708215.25);
}

}  // namespace
