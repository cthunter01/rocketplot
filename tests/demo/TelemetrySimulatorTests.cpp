#include "TelemetrySimulator.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::demo::TelemetryEvent;
using rocketplot::demo::TelemetrySample;
using rocketplot::demo::TelemetrySimulator;

// A whole flight: every reading and event until the vehicle is in orbit.
struct Flight
{
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent>  events;

    Flight()
    {
        TelemetrySimulator simulator;
        simulator.advance(2000.0, samples, events);
    }

    // When the event called @p name happened; NaN if it didn't.
    [[nodiscard]] double timeOf(std::string_view name) const
    {
        const auto event = std::ranges::find(events, name, &TelemetryEvent::name);
        return event == events.end() ? std::numeric_limits<double>::quiet_NaN() : event->time;
    }
};

TEST(TelemetrySimulator, ReportsTwentyTimesASecondOfFlight)
{
    TelemetrySimulator           simulator;
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent>  events;

    simulator.advance(10.0, samples, events);

    ASSERT_EQ(samples.size(), 200U);
    EXPECT_DOUBLE_EQ(samples.front().time, 0.0);
    EXPECT_NEAR(samples.back().time, 9.95, 1e-9);
    EXPECT_NEAR(simulator.time(), 10.0, 1e-9);
    EXPECT_TRUE(std::ranges::is_sorted(samples, {}, &TelemetrySample::time));
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events.front().name, "Liftoff");
}

TEST(TelemetrySimulator, FliesTheSameHoweverTheTimeIsCutUp)
{
    TelemetrySimulator           whole;
    TelemetrySimulator           pieces;
    std::vector<TelemetrySample> once;
    std::vector<TelemetrySample> bitByBit;
    std::vector<TelemetryEvent>  events;

    whole.advance(200.0, once, events);
    for (int tick = 0; tick < 6000; ++tick)
    {
        pieces.advance(1.0 / 30.0, bitByBit, events);
    }

    const std::size_t common = std::min(once.size(), bitByBit.size());
    ASSERT_GT(common, 3990U);  // a rounding error in the time flown may cost the last reading
    EXPECT_DOUBLE_EQ(once[common - 1].altitude, bitByBit[common - 1].altitude);
    EXPECT_DOUBLE_EQ(once[common - 1].velocity, bitByBit[common - 1].velocity);
    EXPECT_DOUBLE_EQ(once[common - 1].acceleration, bitByBit[common - 1].acceleration);
}

TEST(TelemetrySimulator, ReachesOrbit)
{
    const Flight flight;

    const TelemetrySample& last = flight.samples.back();
    EXPECT_GT(last.time, 400.0);
    EXPECT_LT(last.time, 700.0);
    EXPECT_NEAR(last.altitude, 200.0, 20.0);
    EXPECT_NEAR(last.velocity, 7790.0, 100.0);
    EXPECT_GT(last.downrange, 1000.0);
}

TEST(TelemetrySimulator, ReportsTheEventsOfTheFlightInOrder)
{
    const Flight flight;

    std::vector<std::string> names;
    names.reserve(flight.events.size());
    for (const TelemetryEvent& event : flight.events)
    {
        names.emplace_back(event.name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Liftoff", "Max Q", "MECO", "Stage separation",
                                               "SES-1", "Fairing separation", "SECO-1"}));
    EXPECT_TRUE(std::ranges::is_sorted(flight.events, {}, &TelemetryEvent::time));
    EXPECT_DOUBLE_EQ(flight.timeOf("Liftoff"), 0.0);
    EXPECT_NEAR(flight.timeOf("SECO-1"), flight.samples.back().time, 0.1);
}

TEST(TelemetrySimulator, MaxQIsWhereTheDynamicPressurePeaks)
{
    const Flight flight;

    const auto peak =
        std::ranges::max_element(flight.samples, {}, &TelemetrySample::dynamicPressure);
    EXPECT_NEAR(flight.timeOf("Max Q"), peak->time, 0.2);
    EXPECT_GT(peak->dynamicPressure, 20.0);
    EXPECT_LT(peak->dynamicPressure, 60.0);
}

TEST(TelemetrySimulator, TheEnginesThrottleToStayUnderFourG)
{
    const Flight flight;

    const auto hardest =
        std::ranges::max_element(flight.samples, {}, &TelemetrySample::acceleration);
    EXPECT_GT(hardest->acceleration, 3.8);
    EXPECT_LT(hardest->acceleration, 4.2);
}

TEST(TelemetrySimulator, CoastsBetweenTheStages)
{
    const Flight flight;

    const double cutoff   = flight.timeOf("MECO");
    const double ignition = flight.timeOf("SES-1");
    ASSERT_GT(ignition, cutoff + 5.0);
    for (const TelemetrySample& sample : flight.samples)
    {
        if (sample.time > cutoff + 0.1 && sample.time < ignition - 0.1)
        {
            ASSERT_NEAR(sample.acceleration, 0.0, 0.15) << "at T+" << sample.time;
        }
    }
}

TEST(TelemetrySimulator, NothingMoreHappensInOrbit)
{
    TelemetrySimulator           simulator;
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent>  events;
    simulator.advance(2000.0, samples, events);
    ASSERT_TRUE(simulator.isInOrbit());
    const std::size_t readings = samples.size();
    const double      time     = simulator.time();

    simulator.advance(100.0, samples, events);

    EXPECT_EQ(samples.size(), readings);
    EXPECT_DOUBLE_EQ(simulator.time(), time);
}

TEST(TelemetrySimulator, OnlyFliesForward)
{
    TelemetrySimulator           simulator;
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent>  events;

    simulator.advance(-5.0, samples, events);
    simulator.advance(std::numeric_limits<double>::quiet_NaN(), samples, events);
    simulator.advance(0.0, samples, events);

    EXPECT_TRUE(samples.empty());
    EXPECT_DOUBLE_EQ(simulator.time(), 0.0);
    EXPECT_FALSE(simulator.isInOrbit());
}

TEST(TelemetrySimulator, TheSeedOnlyChangesTheNoise)
{
    TelemetrySimulator           first(1);
    TelemetrySimulator           second(2);
    std::vector<TelemetrySample> one;
    std::vector<TelemetrySample> other;
    std::vector<TelemetryEvent>  events;

    first.advance(60.0, one, events);
    second.advance(60.0, other, events);

    ASSERT_EQ(one.size(), other.size());
    EXPECT_DOUBLE_EQ(one.back().altitude, other.back().altitude);
    EXPECT_NE(one.back().acceleration, other.back().acceleration);
    EXPECT_NEAR(one.back().acceleration, other.back().acceleration, 0.2);
}

}  // namespace
