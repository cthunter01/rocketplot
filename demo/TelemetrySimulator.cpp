#include "TelemetrySimulator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

namespace rocketplot::demo
{

namespace
{

constexpr double kStep        = 1.0 / TelemetrySimulator::kSampleRate;
constexpr double kSlack       = 1e-6;     // of a step: 200 steps of 0.05 s are 10 s, not 9.999...
constexpr double kGravity     = 9.80665;  // m/s² at the surface
constexpr double kEarthRadius = 6.371e6;  // m
constexpr double kAirDensity  = 1.225;    // kg/m³ at the surface
constexpr double kScaleHeight = 8500.0;   // m: the air thins by e every scale height
constexpr double kDragArea    = 0.3 * 10.5;  // drag coefficient × frontal area (m²)

// A medium-lift launcher, roughly.
constexpr double kFirstDryMass     = 25'600.0;  // kg
constexpr double kFirstPropellant  = 395'700.0;
constexpr double kFirstThrust      = 7.6e6;  // N
constexpr double kFirstImpulse     = 300.0;  // specific impulse (s)
constexpr double kSecondDryMass    = 4'000.0;
constexpr double kSecondPropellant = 107'500.0;
constexpr double kSecondThrust     = 934e3;
constexpr double kSecondImpulse    = 348.0;
constexpr double kPayload          = 15'000.0;
constexpr double kFairing          = 1'700.0;

constexpr double kMaxAcceleration = 4.0 * kGravity;  // the engines throttle down to hold this
constexpr double kNoise           = 0.02;            // of the accelerometer, in g

// The first stage climbs straight up, then leans over steadily until it burns out.
constexpr double kPitchStart = 10.0;   // s
constexpr double kPitchEnd   = 150.0;  // s
constexpr double kFinalPitch = 22.0;   // degrees above the horizon

constexpr double kSeparationDelay = 3.0;    // s after the first stage's cutoff
constexpr double kIgnitionDelay   = 10.0;   // s after the first stage's cutoff
constexpr double kFairingAltitude = 110e3;  // m: above the air that would heat the payload

// The second stage steers to stop climbing at the target altitude.
constexpr double kTargetAltitude = 200e3;  // m
constexpr double kAltitudeGain   = 0.0006;
constexpr double kClimbGain      = 0.06;

double radians(double degrees)
{
    return degrees * std::numbers::pi / 180.0;
}

}  // namespace

TelemetrySimulator::TelemetrySimulator(std::uint64_t seed)
  : m_noise(seed), m_firstPropellant(kFirstPropellant), m_secondPropellant(kSecondPropellant)
{
}

void TelemetrySimulator::advance(double seconds, std::vector<TelemetrySample>& samples,
                                 std::vector<TelemetryEvent>& events)
{
    if (!std::isfinite(seconds) || seconds <= 0.0 || isInOrbit())
    {
        return;
    }
    // Steps are counted, not their lengths added up: after any number of calls, as many readings
    // have been taken as the time asked for holds (give or take a rounding error at a boundary).
    m_requested += seconds;
    const auto due = static_cast<std::int64_t>(std::floor((m_requested * kSampleRate) + kSlack));
    while (m_steps < due && !isInOrbit())
    {
        step(samples, events);
    }
}

double TelemetrySimulator::totalMass() const noexcept
{
    double mass = kSecondDryMass + m_secondPropellant + kPayload;
    if (m_stagesJoined)
    {
        mass += kFirstDryMass + m_firstPropellant;
    }
    if (m_fairingAttached)
    {
        mass += kFairing;
    }
    return mass;
}

double TelemetrySimulator::engineThrust(double mass) const noexcept
{
    switch (m_phase)
    {
        case Phase::FIRST_STAGE:
            return std::min(kFirstThrust, kMaxAcceleration * mass);
        case Phase::SECOND_STAGE:
            return std::min(kSecondThrust, kMaxAcceleration * mass);
        case Phase::COAST:
        case Phase::ORBIT:
            break;
    }
    return 0.0;
}

double TelemetrySimulator::pitch(double thrustAcceleration, double gravity) const noexcept
{
    if (m_phase != Phase::SECOND_STAGE)
    {
        if (m_time < kPitchStart)
        {
            return radians(90.0);
        }
        const double progress = std::min(1.0, (m_time - kPitchStart) / (kPitchEnd - kPitchStart));
        return radians(90.0 - ((90.0 - kFinalPitch) * std::sin(progress * std::numbers::pi / 2.0)));
    }
    // Enough upward thrust to cancel what gravity the speed doesn't, corrected toward the target
    // altitude and a level flight path.
    const double weightless =
        (m_groundSpeed * m_groundSpeed) / (kEarthRadius + m_altitude);  // centrifugal relief
    const double wanted = (kAltitudeGain * (kTargetAltitude - m_altitude)) -
                          (kClimbGain * m_climbRate) + gravity - weightless;
    return std::asin(std::clamp(wanted / thrustAcceleration, -0.5, 0.9));
}

void TelemetrySimulator::step(std::vector<TelemetrySample>& samples,
                              std::vector<TelemetryEvent>&  events)
{
    if (!m_liftoffReported)
    {
        m_liftoffReported = true;
        events.push_back({.time = 0.0, .name = "Liftoff"});
    }
    const double mass     = totalMass();
    const double radius   = kEarthRadius + m_altitude;
    const double gravity  = kGravity * (kEarthRadius / radius) * (kEarthRadius / radius);
    const double speed    = std::hypot(m_groundSpeed, m_climbRate);
    const double density  = kAirDensity * std::exp(-m_altitude / kScaleHeight);
    const double pressure = 0.5 * density * speed * speed;  // Pa
    const double drag     = pressure * kDragArea;
    const double thrust   = engineThrust(mass);
    const double angle    = thrust > 0.0 ? pitch(thrust / mass, gravity) : 0.0;

    std::normal_distribution<double> noise(0.0, kNoise);
    samples.push_back({
        .time            = m_time,
        .altitude        = m_altitude / 1000.0,
        .downrange       = m_downrange / 1000.0,
        .velocity        = speed,
        .acceleration    = ((thrust - drag) / mass / kGravity) + noise(m_noise),
        .dynamicPressure = pressure / 1000.0,
    });

    // Drag acts against the direction of flight.
    const double dragAlong = speed > 0.0 ? drag * m_groundSpeed / speed : 0.0;
    const double dragUp    = speed > 0.0 ? drag * m_climbRate / speed : 0.0;
    const double along     = ((thrust * std::cos(angle)) - dragAlong) / mass;
    double       up        = (((thrust * std::sin(angle)) - dragUp) / mass) - gravity +
                             ((m_groundSpeed * m_groundSpeed) / radius);
    if (m_altitude <= 0.0)
    {
        up = std::max(up, 0.0);  // the pad holds the vehicle up
    }
    m_groundSpeed += along * kStep;
    m_climbRate += up * kStep;
    m_downrange += m_groundSpeed * kStep;
    m_altitude += m_climbRate * kStep;
    ++m_steps;
    m_time = static_cast<double>(m_steps) * kStep;

    if (!m_passedMaxQ && m_time > 20.0 && pressure < m_lastPressure)
    {
        m_passedMaxQ = true;
        events.push_back({.time = m_time - kStep, .name = "Max Q"});
    }
    m_lastPressure       = pressure;
    const double impulse = m_phase == Phase::FIRST_STAGE ? kFirstImpulse : kSecondImpulse;
    updatePhase(thrust / (impulse * kGravity) * kStep, events);
}

void TelemetrySimulator::updatePhase(double burned, std::vector<TelemetryEvent>& events)
{
    switch (m_phase)
    {
        case Phase::FIRST_STAGE:
            m_firstPropellant -= burned;
            if (m_firstPropellant <= 0.0)
            {
                m_firstPropellant = 0.0;
                m_phase           = Phase::COAST;
                m_cutoffTime      = m_time;
                events.push_back({.time = m_time, .name = "MECO"});
            }
            break;
        case Phase::COAST:
            if (m_stagesJoined && m_time >= m_cutoffTime + kSeparationDelay)
            {
                m_stagesJoined = false;
                events.push_back({.time = m_time, .name = "Stage separation"});
            }
            if (m_time >= m_cutoffTime + kIgnitionDelay)
            {
                m_phase = Phase::SECOND_STAGE;
                events.push_back({.time = m_time, .name = "SES-1"});
            }
            break;
        case Phase::SECOND_STAGE:
        {
            m_secondPropellant -= burned;
            if (m_fairingAttached && m_altitude > kFairingAltitude)
            {
                m_fairingAttached = false;
                events.push_back({.time = m_time, .name = "Fairing separation"});
            }
            const double radius  = kEarthRadius + m_altitude;
            const double orbital = std::sqrt(kGravity * kEarthRadius * kEarthRadius / radius);
            if (m_groundSpeed >= orbital || m_secondPropellant <= 0.0)
            {
                m_phase = Phase::ORBIT;
                events.push_back({.time = m_time, .name = "SECO-1"});
            }
            break;
        }
        case Phase::ORBIT:
            break;
    }
}

}  // namespace rocketplot::demo
