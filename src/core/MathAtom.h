#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/MathSymbols.h"

namespace rocketplot::core
{

/// TeX's styles: how large a formula's parts are set (TeXbook chapter 17). Scripts are a style
/// smaller than their base; a fraction's parts one smaller in text style.
enum class MathStyle : std::uint8_t
{
    DISPLAY,
    TEXT,
    SCRIPT,
    SCRIPT_SCRIPT,
};

/// What an atom of a math list is.
enum class AtomKind : std::uint8_t
{
    SYMBOL,         // a character: x, 2, +, \alpha, \sum
    TEXT,           // upright text: \text{if}, \sin, \operatorname{tr}
    GROUP,          // {...}: a list that is one atom to what is around it
    FRACTION,       // \frac{body}{lower}; \binom, without a rule and between delimiters
    RADICAL,        // \sqrt[lower]{body}
    ACCENT,         // \hat{body}
    OVERLINE,       // \overline{body}
    UNDERLINE,      // \underline{body}
    DELIMITED,      // \left( body \right)
    BIG_DELIMITER,  // \big(, \Bigl[
    ARRAY,          // the cells of a matrix or cases environment
    SPACE,          // \, \quad
    STYLE,          // \displaystyle: the style of the rest of the list
};

struct MathAtom;

/// A formula as TeX's math list: atoms one after another (TeXbook chapter 17).
using MathList = std::vector<MathAtom>;

/// An atom of a math list, with its scripts. One struct for every kind: the fields a kind doesn't
/// use stay empty.
struct MathAtom
{
    AtomKind     kind      = AtomKind::SYMBOL;
    AtomType     type      = AtomType::ORD;  // the class, for the spacing
    char32_t     codePoint = 0;              // SYMBOL, ACCENT (the accent), BIG_DELIMITER
    MathAlphabet alphabet  = MathAlphabet::DEFAULT;

    // The members without a default are given one, so that designated initializers can leave
    // them out.
    // NOLINTBEGIN(readability-redundant-member-init)
    std::string text{};   // TEXT, in UTF-8
    MathList    body{};   // GROUP, numerator, radicand, accented base, DELIMITED's contents
    MathList    lower{};  // denominator, the degree of a radical
    MathList    superscript{};
    MathList    subscript{};
    std::vector<std::vector<MathList>> cells{};  // ARRAY: rows of cells
    std::optional<MathStyle>           style{};  // STYLE; a \dfrac or \tfrac's style
    // NOLINTEND(readability-redundant-member-init)

    char32_t     left        = 0;  // DELIMITED, a \binom or matrix's delimiters; 0 for none
    char32_t     right       = 0;
    double       space       = 0.0;                // SPACE, in mu
    Limits       limits      = Limits::NO_LIMITS;  // OP: where its scripts go
    std::uint8_t size        = 0;                  // BIG_DELIMITER: 1 to 4 for \big to \Bigg
    bool         rule        = true;   // FRACTION: with a fraction bar (\binom has none)
    bool         wide        = false;  // ACCENT: stretched over its base
    bool         leftAligned = false;  // ARRAY: columns to the left (cases)
    std::size_t  position    = 0;      // where it starts in the TeX, in bytes
};

}  // namespace rocketplot::core
