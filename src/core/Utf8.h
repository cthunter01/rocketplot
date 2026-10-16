#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace rocketplot::core
{

/// The character at @p position of the UTF-8 text @p text, moving @p position past it. A byte
/// that doesn't start a valid sequence is U+FFFD, the replacement character, one byte long.
[[nodiscard]] char32_t decodeUtf8(std::string_view text, std::size_t& position);

/// @p text as characters.
[[nodiscard]] std::u32string toUtf32(std::string_view text);

/// Appends @p character to @p text in UTF-8.
void appendUtf8(std::string& text, char32_t character);

}  // namespace rocketplot::core
