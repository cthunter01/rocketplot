#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/// Synthetic data for the demo pages. Every generator is seeded, so pages look the same on every
/// run.
namespace rocketplot::demo
{

/// @p count values from @p start to @p stop inclusive.
[[nodiscard]] std::vector<double> linspace(double start, double stop, std::size_t count);

/// A random walk starting at 0 whose steps are uniform in [-stepSize, stepSize].
[[nodiscard]] std::vector<double> randomWalk(std::size_t count, double stepSize,
                                             std::uint64_t seed);

/// Normally distributed values.
[[nodiscard]] std::vector<double> gaussian(std::size_t count, double mean, double sigma,
                                           std::uint64_t seed);

/// A sensor warming up: from @p base toward base + @p rise with time constant @p tau, plus noise.
[[nodiscard]] std::vector<double> warmUp(const std::vector<double>& time, double base, double rise,
                                         double tau, double noise, std::uint64_t seed);

/// A simulated two-stage ascent: altitude (km), vertical velocity (m/s) and measured acceleration
/// (m/s², with sensor noise), sampled at @p rate Hz for @p duration seconds. Stage 1 burns until
/// 150 s (MECO), the vehicle coasts for 10 s, then stage 2 burns.
struct Ascent
{
    std::vector<double> time;
    std::vector<double> altitude;
    std::vector<double> velocity;
    std::vector<double> acceleration;
};
[[nodiscard]] Ascent simulateAscent(double duration, double rate, std::uint64_t seed);

/// Replaces @p runs stretches of up to @p maxLength values with NaN, at random places.
void addDropouts(std::vector<double>& values, std::size_t runs, std::size_t maxLength,
                 std::uint64_t seed);

}  // namespace rocketplot::demo
