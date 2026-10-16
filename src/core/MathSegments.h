#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rocketplot::core
{

/// A piece of a label: text, or the TeX source of a formula (without its $ signs).
struct MathSegment
{
    std::string text;
    bool        math = false;

    friend bool operator==(const MathSegment&, const MathSegment&) = default;
};

/// Splits @p text (UTF-8) into text and the formulas between $ signs, as matplotlib does: a string
/// with an even number of unescaped $ alternates between text and math; with an odd number (one
/// price, "Cost ($)") it is all text. A $ after a backslash is a literal $: the backslash goes
/// from text segments, and stays in formulas for the TeX parser. Empty segments are left out.
[[nodiscard]] std::vector<MathSegment> splitMath(std::string_view text);

}  // namespace rocketplot::core
