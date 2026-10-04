#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Data for the guide's examples: made up, but shaped like the real thing, and the same on every
// run.
namespace rocketplot::guide
{

/// @p count values from @p start to @p stop inclusive.
[[nodiscard]] std::vector<double> linspace(double start, double stop, std::size_t count);

/// Normally distributed noise around 0.
[[nodiscard]] std::vector<double> noise(std::size_t count, double sigma, std::uint64_t seed);

/// A two-stage ascent sampled at 10 Hz for 300 s: the first stage burns until 150 s, the vehicle
/// coasts, and the second stage lights at 160 s.
struct Ascent
{
    std::vector<double> time;          ///< s
    std::vector<double> altitude;      ///< km
    std::vector<double> velocity;      ///< m/s
    std::vector<double> acceleration;  ///< m/s², as a noisy accelerometer reads it
};
[[nodiscard]] Ascent sampleAscent();

/// A temperature that settles from @p start toward @p end with time constant @p tau, plus
/// sensor noise of @p sigma.
[[nodiscard]] std::vector<double> settling(const std::vector<double>& time, double start,
                                           double end, double tau, double sigma,
                                           std::uint64_t seed);

}  // namespace rocketplot::guide
