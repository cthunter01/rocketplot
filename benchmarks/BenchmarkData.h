#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

/// Data for the benchmarks. Everything is seeded, so that every run measures the same work.
namespace rocketplot::bench
{

/// 0, 1, 2, ...: a time base.
[[nodiscard]] inline std::vector<double> ramp(std::size_t count)
{
    std::vector<double> values(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        values[i] = static_cast<double>(i);
    }
    return values;
}

/// A random walk: noisy the way sensor data is, so that every pixel column of a plot of it holds
/// many ups and downs.
[[nodiscard]] inline std::vector<double> walk(std::size_t count, std::uint64_t seed)
{
    std::mt19937_64                        engine(seed);
    std::uniform_real_distribution<double> step(-1.0, 1.0);
    std::vector<double>                    values(count);
    double                                 value = 0.0;
    for (double& out : values)
    {
        value += step(engine);
        out = value;
    }
    return values;
}

/// Values spread evenly at random from @p low to @p high.
[[nodiscard]] inline std::vector<double> uniform(std::size_t count, double low, double high,
                                                 std::uint64_t seed)
{
    std::mt19937_64                        engine(seed);
    std::uniform_real_distribution<double> value(low, high);
    std::vector<double>                    values(count);
    for (double& out : values)
    {
        out = value(engine);
    }
    return values;
}

/// A curve whose x goes back and forth, so that it isn't sorted by x: the slow path of a line.
struct Curve
{
    std::vector<double> x;
    std::vector<double> y;
};
[[nodiscard]] inline Curve lissajous(std::size_t count)
{
    Curve curve{.x = std::vector<double>(count), .y = std::vector<double>(count)};
    for (std::size_t i = 0; i < count; ++i)
    {
        const double phase =
            2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(count);
        curve.x[i] = std::sin(3.0 * phase);
        curve.y[i] = std::sin(4.0 * phase);
    }
    return curve;
}

}  // namespace rocketplot::bench
