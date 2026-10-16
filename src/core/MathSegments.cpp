#include "core/MathSegments.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rocketplot::core
{

namespace
{

// Whether the $ at @p index is escaped by a backslash.
bool escaped(std::string_view text, std::size_t index)
{
    return index > 0 && text[index - 1] == '\\';
}

// @p text with each \$ turned into $.
std::string unescaped(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '$')
        {
            continue;
        }
        result += text[i];
    }
    return result;
}

void append(std::vector<MathSegment>& segments, std::string text, bool math)
{
    if (!text.empty())
    {
        segments.push_back({.text = std::move(text), .math = math});
    }
}

}  // namespace

std::vector<MathSegment> splitMath(std::string_view text)
{
    // Bytes, not characters: in UTF-8 no byte of a multi-byte character is '$' or '\'.
    std::vector<std::size_t> dollars;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '$' && !escaped(text, i))
        {
            dollars.push_back(i);
        }
    }

    std::vector<MathSegment> segments;
    if (dollars.empty() || dollars.size() % 2 != 0)
    {
        append(segments, unescaped(text), false);
        return segments;
    }
    std::size_t start = 0;
    for (std::size_t i = 0; i < dollars.size(); i += 2)
    {
        const std::size_t open  = dollars[i];
        const std::size_t close = dollars[i + 1];
        append(segments, unescaped(text.substr(start, open - start)), false);
        append(segments, std::string(text.substr(open + 1, close - open - 1)), true);
        start = close + 1;
    }
    append(segments, unescaped(text.substr(start)), false);
    return segments;
}

}  // namespace rocketplot::core
