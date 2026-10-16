#pragma once

#include <cstddef>
#include <span>

namespace rocketplot
{

/// The bytes of Latin Modern Math (src/fonts/latinmodern-math.otf), the font math is set in unless
/// a plot asks for another. Defined in a source file generated from the font
/// (cmake/EmbedFile.cmake).
[[nodiscard]] std::span<const std::byte> bundledMathFont();

}  // namespace rocketplot
