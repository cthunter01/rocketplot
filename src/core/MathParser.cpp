#include "core/MathParser.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/MathAtom.h"
#include "core/MathSymbols.h"
#include "core/Utf8.h"

namespace rocketplot::core
{

namespace
{

constexpr int kMaxDepth = 32;

// In mu: the space of ~ and of "\ ", a space of the text font (a third of an em).
constexpr double kWordSpace = 6.0;

enum class TokenKind : std::uint8_t
{
    END,
    CHARACTER,
    COMMAND,
    OPEN_GROUP,   // {
    CLOSE_GROUP,  // }
    SUPERSCRIPT,  // ^
    SUBSCRIPT,    // _
    ALIGNMENT,    // &
    PRIME,        // '
    TILDE,        // ~
};

struct Token
{
    TokenKind kind      = TokenKind::END;
    char32_t  character = 0;  // CHARACTER
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::string_view name{};  // COMMAND, without its backslash
    std::size_t      position = 0;
};

// The commands that aren't symbols, operator names, alphabets, accents or spaces.
enum class Command : std::uint8_t
{
    FRAC,
    DFRAC,
    TFRAC,
    BINOM,
    DBINOM,
    TBINOM,
    SQRT,
    LEFT,
    BIG,
    OVERLINE,
    UNDERLINE,
    TEXT,
    OPERATORNAME,
    DISPLAYSTYLE,
    TEXTSTYLE,
    SCRIPTSTYLE,
    SCRIPTSCRIPTSTYLE,
    LIMITS,
    NOLIMITS,
    BEGIN,
    NOT,
};

struct CommandEntry
{
    std::string_view name;
    Command          command;
};

constexpr CommandEntry entry(std::string_view name, Command which)
{
    return {.name = name, .command = which};
}

constexpr std::array kCommands = {
    entry("Big", Command::BIG),
    entry("Bigg", Command::BIG),
    entry("Biggl", Command::BIG),
    entry("Biggm", Command::BIG),
    entry("Biggr", Command::BIG),
    entry("Bigl", Command::BIG),
    entry("Bigm", Command::BIG),
    entry("Bigr", Command::BIG),
    entry("begin", Command::BEGIN),
    entry("big", Command::BIG),
    entry("bigg", Command::BIG),
    entry("biggl", Command::BIG),
    entry("biggm", Command::BIG),
    entry("biggr", Command::BIG),
    entry("bigl", Command::BIG),
    entry("bigm", Command::BIG),
    entry("bigr", Command::BIG),
    entry("binom", Command::BINOM),
    entry("dbinom", Command::DBINOM),
    entry("dfrac", Command::DFRAC),
    entry("displaystyle", Command::DISPLAYSTYLE),
    entry("frac", Command::FRAC),
    entry("left", Command::LEFT),
    entry("limits", Command::LIMITS),
    entry("mbox", Command::TEXT),
    entry("nolimits", Command::NOLIMITS),
    entry("not", Command::NOT),
    entry("operatorname", Command::OPERATORNAME),
    entry("overline", Command::OVERLINE),
    entry("scriptscriptstyle", Command::SCRIPTSCRIPTSTYLE),
    entry("scriptstyle", Command::SCRIPTSTYLE),
    entry("sqrt", Command::SQRT),
    entry("tbinom", Command::TBINOM),
    entry("text", Command::TEXT),
    entry("textbf", Command::TEXT),
    entry("textit", Command::TEXT),
    entry("textnormal", Command::TEXT),
    entry("textrm", Command::TEXT),
    entry("textsf", Command::TEXT),
    entry("textstyle", Command::TEXTSTYLE),
    entry("texttt", Command::TEXT),
    entry("tfrac", Command::TFRAC),
    entry("underline", Command::UNDERLINE),
};
static_assert(std::ranges::is_sorted(kCommands, {}, &CommandEntry::name));

std::optional<Command> findCommand(std::string_view name)
{
    // Through a span: std::array's iterators are pointers in some standard libraries only.
    const std::span commands(kCommands);
    const auto      found = std::ranges::lower_bound(commands, name, {}, &CommandEntry::name);
    if (found == commands.end() || found->name != name)
    {
        return std::nullopt;
    }
    return found->command;
}

// An environment: its delimiters, and whether its columns go to the left.
struct Environment
{
    std::string_view name;
    char32_t         left        = 0;
    char32_t         right       = 0;
    bool             leftAligned = false;
};

constexpr std::array kEnvironments = {
    Environment{.name = "Bmatrix", .left = U'{', .right = U'}'},
    Environment{.name = "Vmatrix", .left = 0x2016, .right = 0x2016},
    Environment{.name = "bmatrix", .left = U'[', .right = U']'},
    Environment{.name = "cases", .left = U'{', .right = 0, .leftAligned = true},
    Environment{.name = "matrix"},
    Environment{.name = "pmatrix", .left = U'(', .right = U')'},
    Environment{.name = "vmatrix", .left = U'|', .right = U'|'},
};

// Thrown where the TeX is wrong, and caught by parseMath().
class Failure : public std::exception
{
public:
    explicit Failure(MathError error) : m_error(std::move(error)) { }

    [[nodiscard]] const MathError& error() const { return m_error; }
    [[nodiscard]] const char* what() const noexcept override { return m_error.message.c_str(); }

private:
    MathError m_error;
};

[[noreturn]] void fail(std::size_t position, std::string message)
{
    throw Failure({.position = position, .message = std::move(message)});
}

std::string backslashed(std::string_view name)
{
    return "\\" + std::string(name);
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Where an argument or list ends without having been read: what parseList() stops at.
bool isTerminator(const Token& token)
{
    switch (token.kind)
    {
        case TokenKind::END:
        case TokenKind::CLOSE_GROUP:
        case TokenKind::ALIGNMENT:
            return true;
        case TokenKind::COMMAND:
            return token.name == "\\" || token.name == "right" || token.name == "end";
        default:
            return false;
    }
}

std::string describe(const Token& token)
{
    switch (token.kind)
    {
        case TokenKind::CLOSE_GROUP:
            return "Unexpected }";
        case TokenKind::ALIGNMENT:
            return "& outside a matrix or cases environment";
        case TokenKind::COMMAND:
            if (token.name == "\\")
            {
                return "\\\\ outside a matrix or cases environment";
            }
            if (token.name == "right")
            {
                return "\\right without \\left";
            }
            if (token.name == "end")
            {
                return "\\end without \\begin";
            }
            return "Unexpected " + backslashed(token.name);
        default:
            return "Unexpected end";
    }
}

MathAtom emptyAtom(std::size_t position)
{
    return {.kind = AtomKind::GROUP, .position = position};
}

// Primes as TeX writes them: one character for up to four.
char32_t primes(int count)
{
    switch (count)
    {
        case 2:
            return 0x2033;
        case 3:
            return 0x2034;
        case 4:
            return 0x2057;
        default:
            return 0x2032;
    }
}

class Parser
{
public:
    explicit Parser(std::string_view tex) : m_tex(tex) { }

    MathList parseFormula()
    {
        MathList list = parseList();
        if (const Token token = peek(); token.kind != TokenKind::END)
        {
            fail(token.position, describe(token));
        }
        return list;
    }

private:
    // Counts how deep groups and arguments nest, and stops a formula going too deep.
    class Nesting
    {
    public:
        Nesting(Parser& parser, std::size_t position) : m_parser(&parser)
        {
            if (++m_parser->m_depth > kMaxDepth)
            {
                fail(position, "Nested too deeply");
            }
        }
        ~Nesting() { --m_parser->m_depth; }
        Nesting(const Nesting&)            = delete;
        Nesting& operator=(const Nesting&) = delete;
        Nesting(Nesting&&)                 = delete;
        Nesting& operator=(Nesting&&)      = delete;

    private:
        Parser* m_parser;
    };

    // --- Tokens

    Token lex(std::size_t& position) const
    {
        while (position < m_tex.size() && isSpace(m_tex[position]))
        {
            ++position;
        }
        Token token{.position = position};
        if (position >= m_tex.size())
        {
            return token;
        }
        switch (m_tex[position])
        {
            case '{':
                token.kind = TokenKind::OPEN_GROUP;
                break;
            case '}':
                token.kind = TokenKind::CLOSE_GROUP;
                break;
            case '^':
                token.kind = TokenKind::SUPERSCRIPT;
                break;
            case '_':
                token.kind = TokenKind::SUBSCRIPT;
                break;
            case '&':
                token.kind = TokenKind::ALIGNMENT;
                break;
            case '\'':
                token.kind = TokenKind::PRIME;
                break;
            case '~':
                token.kind = TokenKind::TILDE;
                break;
            case '\\':
                token.kind = TokenKind::COMMAND;
                token.name = commandName(++position);
                return token;
            default:
                token.kind      = TokenKind::CHARACTER;
                token.character = decodeUtf8(m_tex, position);
                return token;
        }
        ++position;
        return token;
    }

    // The name of the command whose backslash is just before @p position: letters, or one other
    // character.
    std::string_view commandName(std::size_t& position) const
    {
        const std::size_t start = position;
        if (position < m_tex.size() && isLetter(m_tex[position]))
        {
            while (position < m_tex.size() && isLetter(m_tex[position]))
            {
                ++position;
            }
        }
        else if (position < m_tex.size())
        {
            (void)decodeUtf8(m_tex, position);
        }
        return m_tex.substr(start, position - start);
    }

    [[nodiscard]] Token peek() const
    {
        std::size_t position = m_position;
        return lex(position);
    }

    Token next() { return lex(m_position); }

    void skipSpaces()
    {
        while (m_position < m_tex.size() && isSpace(m_tex[m_position]))
        {
            ++m_position;
        }
    }

    // Whether the next character is @p c; takes it if it is.
    bool take(char c)
    {
        skipSpaces();
        if (m_position < m_tex.size() && m_tex[m_position] == c)
        {
            ++m_position;
            return true;
        }
        return false;
    }

    // The text of a {...} argument as it is written (\text, environment names), with \{ \} \$
    // and the like as the characters.
    std::string rawArgument()
    {
        skipSpaces();
        const std::size_t open = m_position;
        if (!take('{'))
        {
            fail(m_position, "Missing {");
        }
        std::string text;
        int         depth = 0;
        while (m_position < m_tex.size())
        {
            const char c = m_tex[m_position++];
            if (c == '\\' && m_position < m_tex.size())
            {
                const char escaped = m_tex[m_position];
                if (std::string_view("{}$%&#_ ").contains(escaped))
                {
                    text += escaped;
                    ++m_position;
                    continue;
                }
            }
            if (c == '{')
            {
                ++depth;
            }
            else if (c == '}')
            {
                if (depth == 0)
                {
                    return text;
                }
                --depth;
            }
            text += c;
        }
        fail(open, "Missing }");
    }

    // --- Lists

    // Atoms until a token that ends a list: }, &, \\, \right, \end, or the end.
    MathList parseList()
    {
        MathList list;
        for (Token token = peek(); !isTerminator(token); token = peek())
        {
            parseItem(list);
        }
        return list;
    }

    void parseItem(MathList& list)
    {
        const Token token = next();
        switch (token.kind)
        {
            case TokenKind::CHARACTER:
                list.push_back(symbol(token.character, token.position));
                break;
            case TokenKind::OPEN_GROUP:
            {
                MathAtom group = emptyAtom(token.position);
                group.body     = parseGroup(token.position);
                list.push_back(std::move(group));
                break;
            }
            case TokenKind::SUPERSCRIPT:
            case TokenKind::SUBSCRIPT:
                attachScript(list, token);
                break;
            case TokenKind::PRIME:
                attachPrimes(list, token.position);
                break;
            case TokenKind::TILDE:
                list.push_back(
                    {.kind = AtomKind::SPACE, .space = kWordSpace, .position = token.position});
                break;
            case TokenKind::COMMAND:
                parseCommand(token, list);
                break;
            default:
                fail(token.position, describe(token));
        }
    }

    // The list of a group whose { was at @p open, through its }.
    MathList parseGroup(std::size_t open)
    {
        const Nesting nesting(*this, open);
        MathList      body  = parseList();
        const Token   close = next();
        if (close.kind == TokenKind::END)
        {
            fail(open, "Missing }");
        }
        if (close.kind != TokenKind::CLOSE_GROUP)
        {
            fail(close.position, describe(close));
        }
        return body;
    }

    // An argument: a group, a character or a command (\frac12, x^\alpha).
    MathList parseArgument()
    {
        const Token   token = peek();
        const Nesting nesting(*this, token.position);
        switch (token.kind)
        {
            case TokenKind::OPEN_GROUP:
                next();
                return parseGroup(token.position);
            case TokenKind::CHARACTER:
                next();
                return {symbol(token.character, token.position)};
            case TokenKind::COMMAND:
                if (!isTerminator(token))
                {
                    next();
                    MathList list;
                    parseCommand(token, list);
                    if (!list.empty())
                    {
                        return list;
                    }
                }
                break;
            default:
                break;
        }
        fail(token.position, "Missing argument");
    }

    [[nodiscard]] MathAtom symbol(char32_t character, std::size_t position) const
    {
        return {
            .kind      = AtomKind::SYMBOL,
            .type      = typeOf(character),
            .codePoint = mathCharacter(character),
            .alphabet  = m_alphabet,
            .position  = position,
        };
    }

    // The atom a script goes on: the last of @p list, or a new empty one.
    static MathAtom& base(MathList& list, std::size_t position)
    {
        if (list.empty() || list.back().kind == AtomKind::STYLE ||
            list.back().kind == AtomKind::SPACE)
        {
            list.push_back(emptyAtom(position));
        }
        return list.back();
    }

    void attachScript(MathList& list, const Token& token)
    {
        const bool superscript = token.kind == TokenKind::SUPERSCRIPT;
        MathAtom&  atom        = base(list, token.position);
        MathList&  script      = superscript ? atom.superscript : atom.subscript;
        if (!script.empty())
        {
            fail(token.position, superscript ? "Double superscript" : "Double subscript");
        }
        script = parseArgument();
    }

    // f' is f^{\prime}, f'' f^{\prime\prime}, and f'^2 f^{\prime 2}.
    void attachPrimes(MathList& list, std::size_t position)
    {
        int count = 1;
        while (peek().kind == TokenKind::PRIME)
        {
            next();
            ++count;
        }
        MathAtom& atom = base(list, position);
        if (!atom.superscript.empty())
        {
            fail(position, "Double superscript");
        }
        for (; count > 0; count -= 4)
        {
            atom.superscript.push_back({
                .kind      = AtomKind::SYMBOL,
                .codePoint = primes(std::min(count, 4)),
                .position  = position,
            });
        }
        if (peek().kind == TokenKind::SUPERSCRIPT)
        {
            next();
            MathList more = parseArgument();
            atom.superscript.insert(atom.superscript.end(), std::make_move_iterator(more.begin()),
                                    std::make_move_iterator(more.end()));
        }
    }

    // --- Commands

    void parseCommand(const Token& token, MathList& list)
    {
        const std::string_view name     = token.name;
        const std::size_t      position = token.position;
        if (const auto space = findSpace(name))
        {
            list.push_back({.kind = AtomKind::SPACE, .space = *space, .position = position});
        }
        else if (const auto known = findSymbol(name))
        {
            list.push_back({
                .kind      = AtomKind::SYMBOL,
                .type      = known->type,
                .codePoint = known->codePoint,
                .alphabet  = m_alphabet,
                .limits    = known->limits,
                .position  = position,
            });
        }
        else if (const auto function = findOperatorName(name))
        {
            list.push_back({
                .kind     = AtomKind::TEXT,
                .type     = AtomType::OP,
                .text     = std::string(function->text),
                .limits   = function->limits,
                .position = position,
            });
        }
        else if (const auto alphabet = findAlphabet(name))
        {
            list.push_back(parseAlphabet(*alphabet, position));
        }
        else if (const auto accent = findAccent(name))
        {
            list.push_back({
                .kind      = AtomKind::ACCENT,
                .codePoint = accent->codePoint,
                .body      = parseArgument(),
                .wide      = accent->wide,
                .position  = position,
            });
        }
        else if (const auto other = findCommand(name))
        {
            parseOther(*other, token, list);
        }
        else
        {
            fail(position, "Unknown command " + backslashed(name));
        }
    }

    void parseOther(Command which, const Token& token, MathList& list)
    {
        const std::size_t position = token.position;
        switch (which)
        {
            case Command::FRAC:
            case Command::DFRAC:
            case Command::TFRAC:
            case Command::BINOM:
            case Command::DBINOM:
            case Command::TBINOM:
                list.push_back(parseFraction(which, position));
                break;
            case Command::SQRT:
                list.push_back(parseRadical(position));
                break;
            case Command::LEFT:
                list.push_back(parseDelimited(position));
                break;
            case Command::BIG:
                list.push_back(parseBig(token.name, position));
                break;
            case Command::OVERLINE:
            case Command::UNDERLINE:
                list.push_back({
                    .kind = which == Command::OVERLINE ? AtomKind::OVERLINE : AtomKind::UNDERLINE,
                    .body = parseArgument(),
                    .position = position,
                });
                break;
            case Command::TEXT:
                list.push_back(parseText(token.name, position));
                break;
            case Command::OPERATORNAME:
                list.push_back(parseOperatorName(position));
                break;
            case Command::DISPLAYSTYLE:
            case Command::TEXTSTYLE:
            case Command::SCRIPTSTYLE:
            case Command::SCRIPTSCRIPTSTYLE:
                list.push_back(
                    {.kind = AtomKind::STYLE, .style = styleOf(which), .position = position});
                break;
            case Command::LIMITS:
            case Command::NOLIMITS:
                setLimits(list, which == Command::LIMITS ? Limits::LIMITS : Limits::NO_LIMITS,
                          token);
                break;
            case Command::BEGIN:
                list.push_back(parseEnvironment(position));
                break;
            case Command::NOT:
                list.push_back(parseNot(position));
                break;
        }
    }

    static MathStyle styleOf(Command which)
    {
        switch (which)
        {
            case Command::DISPLAYSTYLE:
                return MathStyle::DISPLAY;
            case Command::SCRIPTSTYLE:
                return MathStyle::SCRIPT;
            case Command::SCRIPTSCRIPTSTYLE:
                return MathStyle::SCRIPT_SCRIPT;
            default:
                return MathStyle::TEXT;
        }
    }

    // \mathbf{...}: the argument with its letters in @p alphabet, as a group.
    MathAtom parseAlphabet(MathAlphabet alphabet, std::size_t position)
    {
        const MathAlphabet outer = std::exchange(m_alphabet, alphabet);
        MathAtom           group = emptyAtom(position);
        group.body               = parseArgument();
        m_alphabet               = outer;
        return group;
    }

    MathAtom parseFraction(Command which, std::size_t position)
    {
        MathAtom fraction{.kind = AtomKind::FRACTION, .position = position};
        fraction.body  = parseArgument();
        fraction.lower = parseArgument();
        if (which == Command::BINOM || which == Command::DBINOM || which == Command::TBINOM)
        {
            fraction.rule  = false;
            fraction.left  = U'(';
            fraction.right = U')';
        }
        if (which == Command::DFRAC || which == Command::DBINOM)
        {
            fraction.style = MathStyle::DISPLAY;
        }
        else if (which == Command::TFRAC || which == Command::TBINOM)
        {
            fraction.style = MathStyle::TEXT;
        }
        return fraction;
    }

    // \sqrt[degree]{radicand}
    MathAtom parseRadical(std::size_t position)
    {
        MathAtom radical{.kind = AtomKind::RADICAL, .position = position};
        if (take('['))
        {
            const Nesting nesting(*this, position);
            for (Token token                                                          = peek();
                 token.kind != TokenKind::CHARACTER || token.character != U']'; token = peek())
            {
                if (isTerminator(token))
                {
                    fail(token.kind == TokenKind::END ? position : token.position, "Missing ]");
                }
                parseItem(radical.lower);
            }
            next();
        }
        radical.body = parseArgument();
        return radical;
    }

    // A delimiter after \left, \right or \big; 0 for the empty one, '.'.
    char32_t parseDelimiter(std::size_t position)
    {
        const Token             token = next();
        std::optional<char32_t> delimiter;
        if (token.kind == TokenKind::CHARACTER)
        {
            delimiter = findDelimiter(token.character);
        }
        else if (token.kind == TokenKind::COMMAND)
        {
            delimiter = findDelimiter(token.name);
        }
        if (!delimiter)
        {
            fail(token.kind == TokenKind::END ? position : token.position, "Missing delimiter");
        }
        return *delimiter;
    }

    MathAtom parseDelimited(std::size_t position)
    {
        const Nesting nesting(*this, position);
        MathAtom      delimited{
            .kind     = AtomKind::DELIMITED,
            .type     = AtomType::INNER,
            .position = position,
        };
        delimited.left  = parseDelimiter(position);
        delimited.body  = parseList();
        const Token end = next();
        if (end.kind != TokenKind::COMMAND || end.name != "right")
        {
            fail(end.kind == TokenKind::END ? position : end.position,
                 end.kind == TokenKind::END ? "\\left without \\right" : describe(end));
        }
        delimited.right = parseDelimiter(end.position);
        return delimited;
    }

    // \big, \Bigl, \biggm and the like: a delimiter of a fixed size.
    MathAtom parseBig(std::string_view name, std::size_t position)
    {
        MathAtom         big{.kind = AtomKind::BIG_DELIMITER, .position = position};
        std::string_view size = name;
        switch (name.back())
        {
            case 'l':
                big.type = AtomType::OPEN;
                size.remove_suffix(1);
                break;
            case 'r':
                big.type = AtomType::CLOSE;
                size.remove_suffix(1);
                break;
            case 'm':
                big.type = AtomType::REL;
                size.remove_suffix(1);
                break;
            default:
                break;
        }
        constexpr std::array<std::string_view, 4> kSizes = {"big", "Big", "bigg", "Bigg"};
        big.size = static_cast<std::uint8_t>(std::ranges::find(kSizes, size) - kSizes.begin() + 1);
        big.codePoint = parseDelimiter(position);
        return big;
    }

    // \text{...} and its kin: upright text, as written.
    MathAtom parseText(std::string_view name, std::size_t position)
    {
        MathAlphabet alphabet = MathAlphabet::UPRIGHT;
        if (name == "textbf")
        {
            alphabet = MathAlphabet::BOLD;
        }
        else if (name == "textit")
        {
            alphabet = MathAlphabet::ITALIC;
        }
        else if (name == "textsf")
        {
            alphabet = MathAlphabet::SANS_SERIF;
        }
        else if (name == "texttt")
        {
            alphabet = MathAlphabet::MONOSPACE;
        }
        return {
            .kind     = AtomKind::TEXT,
            .alphabet = alphabet,
            .text     = rawArgument(),
            .position = position,
        };
    }

    // \operatorname{tr}, or \operatorname*{argmax} with limits.
    MathAtom parseOperatorName(std::size_t position)
    {
        const Limits limits = take('*') ? Limits::DISPLAY : Limits::NO_LIMITS;
        return {
            .kind     = AtomKind::TEXT,
            .type     = AtomType::OP,
            .alphabet = MathAlphabet::UPRIGHT,
            .text     = rawArgument(),
            .limits   = limits,
            .position = position,
        };
    }

    static void setLimits(MathList& list, Limits limits, const Token& token)
    {
        if (list.empty() || list.back().type != AtomType::OP)
        {
            fail(token.position, backslashed(token.name) + " must follow an operator");
        }
        list.back().limits = limits;
    }

    // \not=, \not\in: the relation's negation.
    MathAtom parseNot(std::size_t position)
    {
        const Token             token = next();
        std::optional<char32_t> relation;
        if (token.kind == TokenKind::CHARACTER)
        {
            relation = token.character;
        }
        else if (token.kind == TokenKind::COMMAND)
        {
            if (const auto known = findSymbol(token.name))
            {
                relation = known->codePoint;
            }
        }
        const auto negation = relation ? negated(*relation) : std::nullopt;
        if (!negation)
        {
            fail(position, "\\not of something without a negation");
        }
        return {
            .kind      = AtomKind::SYMBOL,
            .type      = AtomType::REL,
            .codePoint = *negation,
            .position  = position,
        };
    }

    // \begin{name} ... \end{name}: rows of cells, separated by \\ and &.
    MathAtom parseEnvironment(std::size_t position)
    {
        const std::string name = rawArgument();
        const std::span   environments(kEnvironments);
        const auto        environment = std::ranges::find(environments, name, &Environment::name);
        if (environment == environments.end())
        {
            fail(position, "Unknown environment " + name);
        }
        const Nesting nesting(*this, position);
        MathAtom      array{
            .kind        = AtomKind::ARRAY,
            .type        = environment->left != 0 ? AtomType::INNER : AtomType::ORD,
            .left        = environment->left,
            .right       = environment->right,
            .leftAligned = environment->leftAligned,
            .position    = position,
        };
        array.cells.emplace_back();
        while (true)
        {
            array.cells.back().push_back(parseList());
            const Token token = next();
            if (token.kind == TokenKind::ALIGNMENT)
            {
                continue;
            }
            if (token.kind == TokenKind::COMMAND && token.name == "\\")
            {
                array.cells.emplace_back();
                continue;
            }
            if (token.kind == TokenKind::COMMAND && token.name == "end")
            {
                if (rawArgument() != name)
                {
                    fail(token.position, "\\begin{" + name + "} ended by another \\end");
                }
                break;
            }
            fail(token.kind == TokenKind::END ? position : token.position,
                 token.kind == TokenKind::END ? "\\begin{" + name + "} without \\end"
                                              : describe(token));
        }
        // A \\ at the end of the last row makes no row of its own.
        if (array.cells.size() > 1 && array.cells.back().size() == 1 &&
            array.cells.back().front().empty())
        {
            array.cells.pop_back();
        }
        return array;
    }

    std::string_view m_tex;
    std::size_t      m_position = 0;
    int              m_depth    = 0;
    MathAlphabet     m_alphabet = MathAlphabet::DEFAULT;  // of the \mathbf{...} being read
};

}  // namespace

std::expected<MathList, MathError> parseMath(std::string_view tex)
{
    try
    {
        return Parser(tex).parseFormula();
    }
    catch (const Failure& failure)
    {
        return std::unexpected(failure.error());
    }
}

}  // namespace rocketplot::core
