#pragma once

namespace rocketplot
{

/// Implicit, evenly spaced x values: point i is at start + i * step. For uniformly sampled data
/// such as a sensor read at a fixed rate, this stores no x array at all:
/// @code
/// plot->addLine(rocketplot::UniformX{.start = 0.0, .step = 1.0 / 1000.0}, samples, "Accelerometer
/// (1 kHz)");
/// @endcode
struct UniformX
{
    double start = 0.0;
    double step  = 1.0;

    friend constexpr bool operator==(const UniformX&, const UniformX&) = default;
};

}  // namespace rocketplot
