#pragma once

#include <cstdint>
#include <random>
#include <string_view>
#include <vector>

namespace rocketplot::demo
{

/// One reading of every telemetry channel.
struct TelemetrySample
{
    double time            = 0.0;  ///< Mission elapsed time (s)
    double altitude        = 0.0;  ///< km
    double downrange       = 0.0;  ///< km
    double velocity        = 0.0;  ///< m/s
    double acceleration    = 0.0;  ///< g along the vehicle, as its accelerometer reads it (noisy)
    double dynamicPressure = 0.0;  ///< kPa
};

/// Something that happened during the flight.
struct TelemetryEvent
{
    double           time = 0.0;  ///< Mission elapsed time (s)
    std::string_view name;
};

/// A two-stage launch to a 200 km orbit, flown a step at a time: the source of the demo's live
/// telemetry. The first stage flies a fixed pitch program and throttles to stay under 4 g; the
/// second steers to level off at the target altitude and cuts off at orbital speed, about nine
/// minutes after liftoff. The flight is the same every time; only the sensor noise depends on the
/// seed.
class TelemetrySimulator
{
public:
    /// Readings per second of flight.
    static constexpr double kSampleRate = 20.0;

    explicit TelemetrySimulator(std::uint64_t seed = 1);

    /// Flies on for @p seconds, adding the readings taken on the way to @p samples and what
    /// happened to @p events. Nothing more happens once the vehicle is in orbit.
    void advance(double seconds, std::vector<TelemetrySample>& samples,
                 std::vector<TelemetryEvent>& events);

    /// Mission elapsed time in seconds.
    [[nodiscard]] double time() const noexcept { return m_time; }
    /// Whether the second stage has cut off.
    [[nodiscard]] bool isInOrbit() const noexcept { return m_phase == Phase::ORBIT; }

private:
    enum class Phase : std::uint8_t
    {
        FIRST_STAGE,  // burning
        COAST,        // between the first stage's cutoff and the second's start
        SECOND_STAGE,
        ORBIT,
    };

    void step(std::vector<TelemetrySample>& samples, std::vector<TelemetryEvent>& events);
    [[nodiscard]] double totalMass() const noexcept;
    [[nodiscard]] double engineThrust(double mass) const noexcept;
    // The angle of the thrust above the horizon, in radians.
    [[nodiscard]] double pitch(double thrustAcceleration, double gravity) const noexcept;
    void                 updatePhase(double burned, std::vector<TelemetryEvent>& events);

    std::mt19937_64 m_noise;
    Phase           m_phase       = Phase::FIRST_STAGE;
    std::int64_t    m_steps       = 0;    // flown, of 1 / kSampleRate seconds each
    double          m_requested   = 0.0;  // flight time asked for in all
    double          m_time        = 0.0;
    double          m_altitude    = 0.0;      // m
    double          m_downrange   = 0.0;      // m
    double          m_climbRate   = 0.0;      // m/s, up
    double          m_groundSpeed = 0.0;      // m/s, along the ground
    double          m_firstPropellant;        // kg
    double          m_secondPropellant;       // kg
    double          m_cutoffTime      = 0.0;  // of the first stage
    double          m_lastPressure    = 0.0;  // dynamic pressure a step ago
    bool            m_stagesJoined    = true;
    bool            m_fairingAttached = true;
    bool            m_passedMaxQ      = false;
    bool            m_liftoffReported = false;
};

}  // namespace rocketplot::demo
