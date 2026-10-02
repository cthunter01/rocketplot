#include "Generators.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace rocketplot::demo
{

std::vector<double> linspace(double start, double stop, std::size_t count)
{
    std::vector<double> values(count);
    const double        step = count > 1 ? (stop - start) / static_cast<double>(count - 1) : 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        values[i] = start + (static_cast<double>(i) * step);
    }
    return values;
}

std::vector<double> randomWalk(std::size_t count, double stepSize, std::uint64_t seed)
{
    std::mt19937_64                        engine(seed);
    std::uniform_real_distribution<double> step(-stepSize, stepSize);
    std::vector<double>                    values(count);
    double                                 value = 0.0;
    for (double& v : values)
    {
        value += step(engine);
        v = value;
    }
    return values;
}

std::vector<double> gaussian(std::size_t count, double mean, double sigma, std::uint64_t seed)
{
    std::mt19937_64                  engine(seed);
    std::normal_distribution<double> value(mean, sigma);
    std::vector<double>              values(count);
    for (double& v : values)
    {
        v = value(engine);
    }
    return values;
}

std::vector<double> warmUp(const std::vector<double>& time, double base, double rise, double tau,
                           double noise, std::uint64_t seed)
{
    std::vector<double> values = gaussian(time.size(), 0.0, noise, seed);
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        values[i] += base + (rise * (1.0 - std::exp(-time[i] / tau)));
    }
    return values;
}

void addDropouts(std::vector<double>& values, std::size_t runs, std::size_t maxLength,
                 std::uint64_t seed)
{
    if (values.empty())
    {
        return;
    }
    std::mt19937_64                            engine(seed);
    std::uniform_int_distribution<std::size_t> start(0, values.size() - 1);
    std::uniform_int_distribution<std::size_t> length(1, std::max<std::size_t>(1, maxLength));
    for (std::size_t run = 0; run < runs; ++run)
    {
        const std::size_t first = start(engine);
        const std::size_t last  = std::min(values.size(), first + length(engine));
        std::fill(values.begin() + static_cast<std::ptrdiff_t>(first),
                  values.begin() + static_cast<std::ptrdiff_t>(last),
                  std::numeric_limits<double>::quiet_NaN());
    }
}

}  // namespace rocketplot::demo
