#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rocketplot::core
{

/// A color, 8 bits a channel.
struct Rgba
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend constexpr bool operator==(const Rgba&, const Rgba&) = default;
};

/// The built-in color maps.
enum class ColorTable : std::uint8_t
{
    VIRIDIS,    ///< Dark blue through green to yellow; even in lightness, readable in gray
    PLASMA,     ///< Dark blue through red to yellow
    INFERNO,    ///< Black through red to pale yellow
    MAGMA,      ///< Black through purple to pale yellow
    CIVIDIS,    ///< Blue to yellow, the same to most forms of color blindness
    GRAY,       ///< Black to white
    COOL_WARM,  ///< Blue through light gray to red: diverging, for values about a middle
    BLUE_RED,   ///< Blue through white to red, at full strength
};

/// Where a gradient has a color: at @p position, from 0 to 1.
struct GradientStop
{
    double position = 0.0;
    Rgba   color;
};

/// The entries of a lookup table: a color map at 256 evenly spaced points from 0 to 1.
inline constexpr std::size_t kLutSize = 256;
using Lut                             = std::array<Rgba, kLutSize>;

/// The color of @p table at @p t (clamped to 0..1), interpolated between its entries.
[[nodiscard]] Rgba tableAt(ColorTable table, double t);

/// The color of a gradient at @p t: interpolated between the stops either side of it, the first
/// stop's below the first and the last's above the last. @p stops are in increasing position;
/// with none, black.
[[nodiscard]] Rgba gradientAt(std::span<const GradientStop> stops, double t);

/// Lookup tables of a color map and of a gradient, for coloring many values fast.
[[nodiscard]] Lut buildLut(ColorTable table);
[[nodiscard]] Lut buildLut(std::span<const GradientStop> stops);

/// The entry of @p lut for @p t (clamped to 0..1).
[[nodiscard]] Rgba lutAt(const Lut& lut, double t);

}  // namespace rocketplot::core
