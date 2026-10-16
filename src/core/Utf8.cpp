#include "core/Utf8.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace rocketplot::core
{

namespace
{

constexpr char32_t kReplacement = 0xFFFD;

bool continuation(unsigned char byte)
{
    return (byte & 0xC0U) == 0x80U;
}

}  // namespace

char32_t decodeUtf8(std::string_view text, std::size_t& position)
{
    const auto  lead   = static_cast<unsigned char>(text[position]);
    std::size_t length = 0;
    char32_t    value  = 0;
    char32_t    least  = 0;  // the smallest character of this length (longer forms are invalid)
    if (lead < 0x80U)
    {
        ++position;
        return lead;
    }
    if ((lead & 0xE0U) == 0xC0U)
    {
        length = 2;
        value  = lead & 0x1FU;
        least  = 0x80;
    }
    else if ((lead & 0xF0U) == 0xE0U)
    {
        length = 3;
        value  = lead & 0x0FU;
        least  = 0x800;
    }
    else if ((lead & 0xF8U) == 0xF0U)
    {
        length = 4;
        value  = lead & 0x07U;
        least  = 0x10000;
    }
    else
    {
        ++position;
        return kReplacement;
    }
    if (text.size() - position < length)
    {
        ++position;
        return kReplacement;
    }
    for (std::size_t i = 1; i < length; ++i)
    {
        const auto byte = static_cast<unsigned char>(text[position + i]);
        if (!continuation(byte))
        {
            ++position;
            return kReplacement;
        }
        value = (value << 6U) | (byte & 0x3FU);
    }
    if (value < least || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
    {
        ++position;
        return kReplacement;
    }
    position += length;
    return value;
}

std::u32string toUtf32(std::string_view text)
{
    std::u32string result;
    result.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size())
    {
        result += decodeUtf8(text, position);
    }
    return result;
}

void appendUtf8(std::string& text, char32_t character)
{
    const auto byte = [&](char32_t value) { text += static_cast<char>(value); };
    if (character < 0x80)
    {
        byte(character);
    }
    else if (character < 0x800)
    {
        byte(0xC0U | (character >> 6U));
        byte(0x80U | (character & 0x3FU));
    }
    else if (character < 0x10000)
    {
        byte(0xE0U | (character >> 12U));
        byte(0x80U | ((character >> 6U) & 0x3FU));
        byte(0x80U | (character & 0x3FU));
    }
    else
    {
        byte(0xF0U | (character >> 18U));
        byte(0x80U | ((character >> 12U) & 0x3FU));
        byte(0x80U | ((character >> 6U) & 0x3FU));
        byte(0x80U | (character & 0x3FU));
    }
}

}  // namespace rocketplot::core
