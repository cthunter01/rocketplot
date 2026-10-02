#include "rocketplot/plottime.h"

#include <QDateTime>
#include <QTimeZone>
#include <cmath>

namespace rocketplot
{

namespace
{

constexpr double kMillisecondsPerSecond = 1000.0;

}  // namespace

double toPlotTime(const QDateTime& time)
{
    return static_cast<double>(time.toMSecsSinceEpoch()) / kMillisecondsPerSecond;
}

QDateTime fromPlotTime(double seconds)
{
    return QDateTime::fromMSecsSinceEpoch(std::llround(seconds * kMillisecondsPerSecond),
                                          QTimeZone::utc());
}

}  // namespace rocketplot
