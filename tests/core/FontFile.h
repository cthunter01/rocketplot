#pragma once

#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

namespace rocketplot::test
{

/// The bytes of Latin Modern Math, the font the library typesets math with
/// (ROCKETPLOT_MATH_FONT_FILE: src/fonts/latinmodern-math.otf).
inline std::vector<std::byte> mathFontFile()
{
    std::ifstream           file(ROCKETPLOT_MATH_FONT_FILE, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>()};
    std::vector<std::byte>  bytes(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(chars[i]);
    }
    return bytes;
}

}  // namespace rocketplot::test
