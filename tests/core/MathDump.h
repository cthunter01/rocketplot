#pragma once

// A math list as a short string, for tests to compare: atoms separated by spaces, their parts in
// parentheses.
//   x        a symbol (its character; with its alphabet: x.bf)
//   {a b}    a group
//   x_(i)^(2)  scripts
//   frac(a,b) frac[D](1,2) binom(n,k)   sqrt(x) sqrt[3](x)
//   accent[302](x) accent[302w](xy)     overline(x) underline(x)
//   left( x right)   big2l(   array[(,)](a,b;c,d) array[{,.,left](...)
//   text(if) op(sin) op[limits](max)    space(3) style(D)

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include "core/MathAtom.h"
#include "core/MathSymbols.h"
#include "core/Utf8.h"

namespace rocketplot::test
{

inline std::string dump(const core::MathList& list);

inline std::string dumpCharacter(char32_t c)
{
    if (c == 0)
    {
        return ".";
    }
    std::string text;
    core::appendUtf8(text, c);
    return text;
}

inline std::string_view dumpAlphabet(core::MathAlphabet alphabet)
{
    using core::MathAlphabet;
    switch (alphabet)
    {
        case MathAlphabet::DEFAULT:
            return "";
        case MathAlphabet::UPRIGHT:
            return ".rm";
        case MathAlphabet::ITALIC:
            return ".it";
        case MathAlphabet::BOLD:
            return ".bf";
        case MathAlphabet::BOLD_ITALIC:
            return ".bi";
        case MathAlphabet::DOUBLE_STRUCK:
            return ".bb";
        case MathAlphabet::SCRIPT:
            return ".cal";
        case MathAlphabet::FRAKTUR:
            return ".frak";
        case MathAlphabet::SANS_SERIF:
            return ".sf";
        case MathAlphabet::MONOSPACE:
            return ".tt";
    }
    return "";
}

inline std::string_view dumpStyle(core::MathStyle style)
{
    using core::MathStyle;
    switch (style)
    {
        case MathStyle::DISPLAY:
            return "D";
        case MathStyle::TEXT:
            return "T";
        case MathStyle::SCRIPT:
            return "S";
        case MathStyle::SCRIPT_SCRIPT:
            return "SS";
    }
    return "";
}

inline std::string dumpText(const core::MathAtom& atom)
{
    std::string kind = atom.type == core::AtomType::OP ? "op" : "text";
    if (atom.limits == core::Limits::DISPLAY && atom.type == core::AtomType::OP)
    {
        kind += "[limits]";
    }
    return kind + "(" + atom.text + ")" + std::string(dumpAlphabet(atom.alphabet));
}

inline std::string dumpFraction(const core::MathAtom& atom)
{
    std::string text = atom.rule ? "frac" : "binom";
    if (atom.style)
    {
        text += "[" + std::string(dumpStyle(*atom.style)) + "]";
    }
    return text + "(" + dump(atom.body) + "," + dump(atom.lower) + ")";
}

inline std::string dumpBig(const core::MathAtom& atom)
{
    std::string_view side;
    switch (atom.type)
    {
        case core::AtomType::OPEN:
            side = "l";
            break;
        case core::AtomType::CLOSE:
            side = "r";
            break;
        case core::AtomType::REL:
            side = "m";
            break;
        default:
            break;
    }
    return std::format("big{}{}{}", atom.size, side, dumpCharacter(atom.codePoint));
}

inline std::string dumpArray(const core::MathAtom& atom)
{
    std::string text = "array[" + dumpCharacter(atom.left) + "," + dumpCharacter(atom.right) +
                       (atom.leftAligned ? ",left" : "") + "](";
    for (std::size_t row = 0; row < atom.cells.size(); ++row)
    {
        text += row > 0 ? ";" : "";
        for (std::size_t cell = 0; cell < atom.cells[row].size(); ++cell)
        {
            text += (cell > 0 ? "," : "") + dump(atom.cells[row][cell]);
        }
    }
    return text + ")";
}

inline std::string dumpNucleus(const core::MathAtom& atom)
{
    using core::AtomKind;
    switch (atom.kind)
    {
        case AtomKind::SYMBOL:
            return dumpCharacter(atom.codePoint) + std::string(dumpAlphabet(atom.alphabet));
        case AtomKind::TEXT:
            return dumpText(atom);
        case AtomKind::GROUP:
            return "{" + dump(atom.body) + "}";
        case AtomKind::FRACTION:
            return dumpFraction(atom);
        case AtomKind::RADICAL:
            return "sqrt" + (atom.lower.empty() ? std::string() : "[" + dump(atom.lower) + "]") +
                   "(" + dump(atom.body) + ")";
        case AtomKind::ACCENT:
            return std::format("accent[{:x}{}]({})", static_cast<unsigned>(atom.codePoint),
                               atom.wide ? "w" : "", dump(atom.body));
        case AtomKind::OVERLINE:
            return "overline(" + dump(atom.body) + ")";
        case AtomKind::UNDERLINE:
            return "underline(" + dump(atom.body) + ")";
        case AtomKind::DELIMITED:
            return "left" + dumpCharacter(atom.left) + " " + dump(atom.body) + " right" +
                   dumpCharacter(atom.right);
        case AtomKind::BIG_DELIMITER:
            return dumpBig(atom);
        case AtomKind::ARRAY:
            return dumpArray(atom);
        case AtomKind::SPACE:
            return std::format("space({})", atom.space);
        case AtomKind::STYLE:
            return "style(" + std::string(dumpStyle(atom.style.value_or(core::MathStyle::TEXT))) +
                   ")";
    }
    return "?";
}

inline std::string dumpAtom(const core::MathAtom& atom)
{
    std::string text = dumpNucleus(atom);
    if (!atom.subscript.empty())
    {
        text += "_(" + dump(atom.subscript) + ")";
    }
    if (!atom.superscript.empty())
    {
        text += "^(" + dump(atom.superscript) + ")";
    }
    return text;
}

inline std::string dump(const core::MathList& list)
{
    std::string text;
    for (const core::MathAtom& atom : list)
    {
        text += (text.empty() ? "" : " ") + dumpAtom(atom);
    }
    return text;
}

}  // namespace rocketplot::test
