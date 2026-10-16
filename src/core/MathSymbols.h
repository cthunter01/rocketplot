#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rocketplot::core
{

/// TeX's classes of atoms, which decide the space between them (TeXbook chapter 17).
enum class AtomType : std::uint8_t
{
    ORD,    // ordinary: x, 1, \infty
    OP,     // large operator: \sum, \int, \sin
    BIN,    // binary operation: +, \times
    REL,    // relation: =, \leq, \to
    OPEN,   // opening: (, \langle
    CLOSE,  // closing: ), \rangle
    PUNCT,  // punctuation: ,
    INNER,  // a \left...\right group, a fraction among delimiters
};

/// How letters are set: TeX's default (Latin letters and lower case Greek in italic, the rest
/// upright) or one of the alphabets of \mathrm, \mathbf and the like, which are characters of
/// Unicode's Mathematical Alphanumeric Symbols.
enum class MathAlphabet : std::uint8_t
{
    DEFAULT,
    UPRIGHT,        // \mathrm
    ITALIC,         // \mathit
    BOLD,           // \mathbf
    BOLD_ITALIC,    // \boldsymbol
    DOUBLE_STRUCK,  // \mathbb
    SCRIPT,         // \mathcal, \mathscr
    FRAKTUR,        // \mathfrak
    SANS_SERIF,     // \mathsf
    MONOSPACE,      // \mathtt
};

/// Where a large operator's scripts go: above and below it in display style only (\sum), always
/// (\limits), or beside it (\int, \sin, \nolimits).
enum class Limits : std::uint8_t
{
    DISPLAY,
    LIMITS,
    NO_LIMITS,
};

/// A command that stands for a character: \alpha, \leq, \sum.
struct MathSymbol
{
    char32_t codePoint = 0;
    AtomType type      = AtomType::ORD;
    Limits   limits    = Limits::NO_LIMITS;  // for large operators

    friend bool operator==(const MathSymbol&, const MathSymbol&) = default;
};

/// The symbol of command @p name (without its backslash: "alpha"), if it is one.
[[nodiscard]] std::optional<MathSymbol> findSymbol(std::string_view name);

/// The character of a delimiter after \left, \right or \big: a character ('(', '|', '.') or a
/// command (@p command set: "langle", "{", "|"). 0 for the empty delimiter '.'; nullopt if it
/// isn't a delimiter.
[[nodiscard]] std::optional<char32_t> findDelimiter(char32_t character);
[[nodiscard]] std::optional<char32_t> findDelimiter(std::string_view command);

/// An accent command: \hat, \vec, \widehat (wide: it stretches over its base).
struct MathAccent
{
    char32_t codePoint = 0;  // the combining character
    bool     wide      = false;

    friend bool operator==(const MathAccent&, const MathAccent&) = default;
};

[[nodiscard]] std::optional<MathAccent> findAccent(std::string_view name);

/// A function name: \sin, \lim, \liminf. Set upright, as a large operator.
struct OperatorName
{
    std::string_view text;
    Limits           limits = Limits::NO_LIMITS;

    friend bool operator==(const OperatorName&, const OperatorName&) = default;
};

[[nodiscard]] std::optional<OperatorName> findOperatorName(std::string_view name);

/// The alphabet of \mathbf, \mathbb and the like.
[[nodiscard]] std::optional<MathAlphabet> findAlphabet(std::string_view name);

/// The space of a spacing command (\, \quad), in mu (an eighteenth of an em).
[[nodiscard]] std::optional<double> findSpace(std::string_view name);

/// The relation \not makes of @p relation: ≠ of =, ∉ of ∈. Nullopt if Unicode has none.
[[nodiscard]] std::optional<char32_t> negated(char32_t relation);

/// The class of a character typed into a formula: '+' BIN, '=' REL, '(' OPEN, '≤' REL, 'x' ORD.
[[nodiscard]] AtomType typeOf(char32_t character);

/// The character a typed one stands for in math: the minus sign for '-', the asterisk operator
/// for '*'; others are themselves.
[[nodiscard]] char32_t mathCharacter(char32_t character);

/// @p character as set in @p alphabet: 'x' is U+1D465 (𝑥) in the default italic, 'R' in
/// DOUBLE_STRUCK is U+211D (ℝ). Characters an alphabet has no form of stay as they are.
[[nodiscard]] char32_t styledCharacter(char32_t character, MathAlphabet alphabet);

}  // namespace rocketplot::core
