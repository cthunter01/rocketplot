#include "core/MathPlainText.h"

#include <cstddef>
#include <string>

#include "core/MathAtom.h"
#include "core/MathSymbols.h"
#include "core/Utf8.h"

namespace rocketplot::core
{

namespace
{

constexpr char32_t kCombiningOverline = 0x305;
constexpr char32_t kCombiningLowLine  = 0x332;
constexpr char32_t kSquareRoot        = 0x221A;
constexpr double   kThinSpace         = 3.0;  // mu: a space at least this wide is written

class Writer
{
public:
    std::string text;

    void list(const MathList& atoms, bool script)
    {
        for (std::size_t i = 0; i < atoms.size(); ++i)
        {
            const MathAtom& atom   = atoms[i];
            const bool      spaced = !script && spacedAround(atoms, i);
            if (spaced)
            {
                space();
            }
            nucleus(atom);
            scripts(atom);
            if (spaced || (atom.type == AtomType::OP && i + 1 < atoms.size() &&
                           atoms[i + 1].type == AtomType::ORD))
            {
                space();
            }
        }
    }

private:
    void character(char32_t c) { appendUtf8(text, c); }

    void space()
    {
        if (!text.empty() && text.back() != ' ')
        {
            text += ' ';
        }
    }

    // Whether the atom at @p index is a relation or a binary operation between two operands,
    // which reads better with spaces around it: "a + b = c", but "-x".
    static bool spacedAround(const MathList& atoms, std::size_t index)
    {
        const AtomType type = atoms[index].type;
        if (type == AtomType::REL)
        {
            return true;
        }
        if (type != AtomType::BIN || index == 0 || index + 1 == atoms.size())
        {
            return false;
        }
        const AtomType before = atoms[index - 1].type;
        return before != AtomType::BIN && before != AtomType::REL && before != AtomType::OPEN &&
               before != AtomType::PUNCT && before != AtomType::OP;
    }

    // @p atoms, in parentheses if they are more than one thing: "x" but "(i+1)"; with spaces
    // around operators unless @p script.
    void argument(const MathList& atoms, bool script)
    {
        const bool alone =
            atoms.size() == 1 && atoms.front().superscript.empty() &&
            atoms.front().subscript.empty() &&
            (atoms.front().kind == AtomKind::SYMBOL ||
             (atoms.front().kind == AtomKind::GROUP && atoms.front().body.size() == 1));
        if (!alone)
        {
            text += '(';
        }
        list(atoms, script);
        if (!alone)
        {
            text += ')';
        }
    }

    void scripts(const MathAtom& atom)
    {
        if (!atom.subscript.empty())
        {
            text += '_';
            argument(atom.subscript, true);
        }
        if (atom.superscript.empty())
        {
            return;
        }
        // Primes as they are: f′, f′^2.
        std::size_t primes = 0;
        while (primes < atom.superscript.size() && isPrime(atom.superscript[primes]))
        {
            character(atom.superscript[primes++].codePoint);
        }
        if (primes < atom.superscript.size())
        {
            text += '^';
            argument(MathList(atom.superscript.begin() + static_cast<std::ptrdiff_t>(primes),
                              atom.superscript.end()),
                     true);
        }
    }

    static bool isPrime(const MathAtom& atom)
    {
        return atom.kind == AtomKind::SYMBOL &&
               (atom.codePoint == 0x2032 || atom.codePoint == 0x2033 || atom.codePoint == 0x2034 ||
                atom.codePoint == 0x2057);
    }

    // The character of a symbol: as typed, but for the alphabets whose letters mean something
    // else (ℝ isn't R).
    static char32_t plainCharacter(const MathAtom& atom)
    {
        switch (atom.alphabet)
        {
            case MathAlphabet::DOUBLE_STRUCK:
            case MathAlphabet::SCRIPT:
            case MathAlphabet::FRAKTUR:
                return styledCharacter(atom.codePoint, atom.alphabet);
            default:
                return atom.codePoint;
        }
    }

    void delimiter(char32_t c)
    {
        if (c != 0)
        {
            character(c);
        }
    }

    void nucleus(const MathAtom& atom)
    {
        switch (atom.kind)
        {
            case AtomKind::SYMBOL:
            case AtomKind::BIG_DELIMITER:
                character(plainCharacter(atom));
                break;
            case AtomKind::TEXT:
                text += atom.text;
                break;
            case AtomKind::GROUP:
                list(atom.body, false);
                break;
            case AtomKind::FRACTION:
                fraction(atom);
                break;
            case AtomKind::RADICAL:
                radical(atom);
                break;
            case AtomKind::ACCENT:
                list(atom.body, true);
                character(atom.codePoint);
                break;
            case AtomKind::OVERLINE:
            case AtomKind::UNDERLINE:
                list(atom.body, true);
                character(atom.kind == AtomKind::OVERLINE ? kCombiningOverline : kCombiningLowLine);
                break;
            case AtomKind::DELIMITED:
                delimiter(atom.left);
                list(atom.body, false);
                delimiter(atom.right);
                break;
            case AtomKind::ARRAY:
                array(atom);
                break;
            case AtomKind::SPACE:
                if (atom.space >= kThinSpace)
                {
                    space();
                }
                break;
            case AtomKind::STYLE:
                break;
        }
    }

    void fraction(const MathAtom& atom)
    {
        if (!atom.rule)  // \binom
        {
            text += '(';
            list(atom.body, true);
            text += " choose ";
            list(atom.lower, true);
            text += ')';
            return;
        }
        argument(atom.body, false);
        text += '/';
        argument(atom.lower, false);
    }

    void radical(const MathAtom& atom)
    {
        if (!atom.lower.empty())
        {
            list(atom.lower, true);
        }
        character(kSquareRoot);
        argument(atom.body, false);
    }

    // Cells separated by commas, rows by semicolons: "(1, 0; 0, 1)".
    void array(const MathAtom& atom)
    {
        delimiter(atom.left);
        for (std::size_t row = 0; row < atom.cells.size(); ++row)
        {
            if (row > 0)
            {
                text += "; ";
            }
            for (std::size_t cell = 0; cell < atom.cells[row].size(); ++cell)
            {
                if (cell > 0)
                {
                    text += ", ";
                }
                list(atom.cells[row][cell], false);
            }
        }
        delimiter(atom.right);
    }
};

}  // namespace

std::string mathToPlainText(const MathList& list)
{
    Writer writer;
    writer.list(list, false);
    while (!writer.text.empty() && writer.text.back() == ' ')
    {
        writer.text.pop_back();
    }
    return writer.text;
}

}  // namespace rocketplot::core
