#include "SampleData.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace rocketplot::guide
{

namespace
{

constexpr double kGravity          = 9.81;
constexpr double kFirstStageEnd    = 150.0;
constexpr double kSecondStageStart = 160.0;
constexpr double kDuration         = 300.0;
constexpr double kRate             = 10.0;

}  // namespace

std::vector<double> linspace(double start, double stop, std::size_t count)
{
    std::vector<double> values(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double fraction =
            count > 1 ? static_cast<double>(i) / static_cast<double>(count - 1) : 0.0;
        values[i] = start + ((stop - start) * fraction);
    }
    return values;
}

std::vector<double> noise(std::size_t count, double sigma, std::uint64_t seed)
{
    std::vector<double> values(count);
    if (sigma <= 0.0)
    {
        return values;  // no noise at all
    }
    std::mt19937_64                  engine(seed);
    std::normal_distribution<double> value(0.0, sigma);
    for (double& out : values)
    {
        out = value(engine);
    }
    return values;
}

Ascent sampleAscent()
{
    const auto                count  = static_cast<std::size_t>(kDuration * kRate) + 1;
    const std::vector<double> jitter = noise(count, 0.4, 3);
    Ascent                    ascent;
    double                    velocity = 0.0;
    double                    altitude = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double time = static_cast<double>(i) / kRate;
        // The thrust's acceleration grows as the propellant burns off.
        double thrust = 0.0;
        if (time < kFirstStageEnd)
        {
            thrust = 13.0 + (0.12 * time);
        }
        else if (time >= kSecondStageStart)
        {
            thrust = 8.0 + (0.06 * (time - kSecondStageStart));
        }
        const double acceleration = thrust - kGravity;
        velocity += acceleration / kRate;
        altitude += velocity / kRate;
        ascent.time.push_back(time);
        ascent.altitude.push_back(altitude / 1000.0);
        ascent.velocity.push_back(velocity);
        ascent.acceleration.push_back(acceleration + jitter[i]);
    }
    return ascent;
}

std::vector<double> settling(const std::vector<double>& time, double start, double end, double tau,
                             double sigma, std::uint64_t seed)
{
    std::vector<double> values = noise(time.size(), sigma, seed);
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        values[i] += end + ((start - end) * std::exp(-time[i] / tau));
    }
    return values;
}

}  // namespace rocketplot::guide
