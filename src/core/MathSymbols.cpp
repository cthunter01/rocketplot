#include "core/MathSymbols.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace rocketplot::core
{

namespace
{

// A command and what it stands for. The tables are sorted by name, for a binary search.
template <typename Value>
struct Named
{
    std::string_view name;
    Value            value;
};

template <typename Value, std::size_t N>
constexpr bool sortedByName(const std::array<Named<Value>, N>& table)
{
    return std::ranges::is_sorted(table, {}, &Named<Value>::name);
}

template <typename Value, std::size_t N>
std::optional<Value> find(const std::array<Named<Value>, N>& table, std::string_view name)
{
    // Through a span: its iterators are of a class everywhere, where std::array's are pointers in
    // some standard libraries and not in others.
    const std::span entries(table);
    const auto      found = std::ranges::lower_bound(entries, name, {}, &Named<Value>::name);
    if (found == entries.end() || found->name != name)
    {
        return std::nullopt;
    }
    return found->value;
}

using enum AtomType;
using enum Limits;
using enum MathAlphabet;

// The tables' entries.
constexpr Named<MathSymbol> symbol(std::string_view name, char32_t codePoint, AtomType type,
                                   Limits limits = NO_LIMITS)
{
    return {.name = name, .value = {.codePoint = codePoint, .type = type, .limits = limits}};
}

constexpr Named<char32_t> delimiter(std::string_view name, char32_t codePoint)
{
    return {.name = name, .value = codePoint};
}

constexpr Named<MathAccent> accent(std::string_view name, char32_t codePoint)
{
    return {.name = name, .value = {.codePoint = codePoint, .wide = false}};
}

constexpr Named<MathAccent> wideAccent(std::string_view name, char32_t codePoint)
{
    return {.name = name, .value = {.codePoint = codePoint, .wide = true}};
}

constexpr Named<OperatorName> function(std::string_view name, std::string_view text, Limits limits)
{
    return {.name = name, .value = {.text = text, .limits = limits}};
}

constexpr Named<MathAlphabet> font(std::string_view name, MathAlphabet value)
{
    return {.name = name, .value = value};
}

constexpr Named<double> space(std::string_view name, double mu)
{
    return {.name = name, .value = mu};
}

// clang-format off
constexpr std::array kSymbols = {
    symbol("#", 0x0023, ORD),
    symbol("$", 0x0024, ORD),
    symbol("%", 0x0025, ORD),
    symbol("&", 0x0026, ORD),
    symbol("Box", 0x25A1, ORD),
    symbol("Delta", 0x0394, ORD),
    symbol("Downarrow", 0x21D3, REL),
    symbol("Gamma", 0x0393, ORD),
    symbol("Im", 0x2111, ORD),
    symbol("Lambda", 0x039B, ORD),
    symbol("Leftarrow", 0x21D0, REL),
    symbol("Leftrightarrow", 0x21D4, REL),
    symbol("Longleftarrow", 0x27F8, REL),
    symbol("Longleftrightarrow", 0x27FA, REL),
    symbol("Longrightarrow", 0x27F9, REL),
    symbol("Omega", 0x03A9, ORD),
    symbol("Phi", 0x03A6, ORD),
    symbol("Pi", 0x03A0, ORD),
    symbol("Psi", 0x03A8, ORD),
    symbol("Re", 0x211C, ORD),
    symbol("Rightarrow", 0x21D2, REL),
    symbol("Sigma", 0x03A3, ORD),
    symbol("Theta", 0x0398, ORD),
    symbol("Uparrow", 0x21D1, REL),
    symbol("Updownarrow", 0x21D5, REL),
    symbol("Upsilon", 0x03A5, ORD),
    symbol("Vert", 0x2016, ORD),
    symbol("Xi", 0x039E, ORD),
    symbol("_", 0x005F, ORD),
    symbol("aleph", 0x2135, ORD),
    symbol("alpha", 0x03B1, ORD),
    symbol("amalg", 0x2A3F, BIN),
    symbol("angle", 0x2220, ORD),
    symbol("approx", 0x2248, REL),
    symbol("approxeq", 0x224A, REL),
    symbol("ast", 0x2217, BIN),
    symbol("asymp", 0x224D, REL),
    symbol("backslash", 0x005C, ORD),
    symbol("beta", 0x03B2, ORD),
    symbol("beth", 0x2136, ORD),
    symbol("bigcap", 0x22C2, OP, DISPLAY),
    symbol("bigcirc", 0x25EF, BIN),
    symbol("bigcup", 0x22C3, OP, DISPLAY),
    symbol("bigodot", 0x2A00, OP, DISPLAY),
    symbol("bigoplus", 0x2A01, OP, DISPLAY),
    symbol("bigotimes", 0x2A02, OP, DISPLAY),
    symbol("bigsqcup", 0x2A06, OP, DISPLAY),
    symbol("bigtriangledown", 0x25BD, BIN),
    symbol("bigtriangleup", 0x25B3, BIN),
    symbol("biguplus", 0x2A04, OP, DISPLAY),
    symbol("bigvee", 0x22C1, OP, DISPLAY),
    symbol("bigwedge", 0x22C0, OP, DISPLAY),
    symbol("bot", 0x22A5, ORD),
    symbol("bowtie", 0x22C8, REL),
    symbol("bullet", 0x2219, BIN),
    symbol("cap", 0x2229, BIN),
    symbol("cdot", 0x22C5, BIN),
    symbol("cdots", 0x22EF, ORD),
    symbol("checkmark", 0x2713, ORD),
    symbol("chi", 0x03C7, ORD),
    symbol("circ", 0x2218, BIN),
    symbol("clubsuit", 0x2663, ORD),
    symbol("colon", 0x003A, PUNCT),
    symbol("coloneqq", 0x2254, REL),
    symbol("cong", 0x2245, REL),
    symbol("coprod", 0x2210, OP, DISPLAY),
    symbol("cup", 0x222A, BIN),
    symbol("dagger", 0x2020, BIN),
    symbol("dashv", 0x22A3, REL),
    symbol("ddagger", 0x2021, BIN),
    symbol("ddots", 0x22F1, ORD),
    symbol("degree", 0x00B0, ORD),
    symbol("delta", 0x03B4, ORD),
    symbol("diamond", 0x22C4, BIN),
    symbol("diamondsuit", 0x2662, ORD),
    symbol("digamma", 0x03DD, ORD),
    symbol("div", 0x00F7, BIN),
    symbol("doteq", 0x2250, REL),
    symbol("dots", 0x2026, ORD),
    symbol("downarrow", 0x2193, REL),
    symbol("ell", 0x2113, ORD),
    symbol("emptyset", 0x2205, ORD),
    symbol("epsilon", 0x03F5, ORD),
    symbol("equiv", 0x2261, REL),
    symbol("eta", 0x03B7, ORD),
    symbol("exists", 0x2203, ORD),
    symbol("flat", 0x266D, ORD),
    symbol("forall", 0x2200, ORD),
    symbol("frown", 0x2322, REL),
    symbol("gamma", 0x03B3, ORD),
    symbol("ge", 0x2265, REL),
    symbol("geq", 0x2265, REL),
    symbol("geqslant", 0x2A7E, REL),
    symbol("gets", 0x2190, REL),
    symbol("gg", 0x226B, REL),
    symbol("gtrsim", 0x2273, REL),
    symbol("hbar", 0x210F, ORD),
    symbol("heartsuit", 0x2661, ORD),
    symbol("hookleftarrow", 0x21A9, REL),
    symbol("hookrightarrow", 0x21AA, REL),
    symbol("iff", 0x27FA, REL),
    symbol("iiint", 0x222D, OP),
    symbol("iint", 0x222C, OP),
    symbol("imath", 0x0131, ORD),
    symbol("implies", 0x27F9, REL),
    symbol("in", 0x2208, REL),
    symbol("infty", 0x221E, ORD),
    symbol("int", 0x222B, OP),
    symbol("iota", 0x03B9, ORD),
    symbol("jmath", 0x0237, ORD),
    symbol("kappa", 0x03BA, ORD),
    symbol("lVert", 0x2016, OPEN),
    symbol("lambda", 0x03BB, ORD),
    symbol("land", 0x2227, BIN),
    symbol("langle", 0x27E8, OPEN),
    symbol("lbrace", 0x007B, OPEN),
    symbol("lceil", 0x2308, OPEN),
    symbol("ldots", 0x2026, ORD),
    symbol("le", 0x2264, REL),
    symbol("leftarrow", 0x2190, REL),
    symbol("leftharpoondown", 0x21BD, REL),
    symbol("leftharpoonup", 0x21BC, REL),
    symbol("leftrightarrow", 0x2194, REL),
    symbol("leq", 0x2264, REL),
    symbol("leqslant", 0x2A7D, REL),
    symbol("lesssim", 0x2272, REL),
    symbol("lfloor", 0x230A, OPEN),
    symbol("ll", 0x226A, REL),
    symbol("lnot", 0x00AC, ORD),
    symbol("longleftarrow", 0x27F5, REL),
    symbol("longleftrightarrow", 0x27F7, REL),
    symbol("longmapsto", 0x27FC, REL),
    symbol("longrightarrow", 0x27F6, REL),
    symbol("lor", 0x2228, BIN),
    symbol("lvert", 0x007C, OPEN),
    symbol("mapsto", 0x21A6, REL),
    symbol("mho", 0x2127, ORD),
    symbol("mid", 0x2223, REL),
    symbol("models", 0x22A8, REL),
    symbol("mp", 0x2213, BIN),
    symbol("mu", 0x03BC, ORD),
    symbol("nabla", 0x2207, ORD),
    symbol("natural", 0x266E, ORD),
    symbol("ne", 0x2260, REL),
    symbol("nearrow", 0x2197, REL),
    symbol("neg", 0x00AC, ORD),
    symbol("neq", 0x2260, REL),
    symbol("nexists", 0x2204, ORD),
    symbol("ni", 0x220B, REL),
    symbol("notin", 0x2209, REL),
    symbol("nu", 0x03BD, ORD),
    symbol("nwarrow", 0x2196, REL),
    symbol("odot", 0x2299, BIN),
    symbol("oiiint", 0x2230, OP),
    symbol("oiint", 0x222F, OP),
    symbol("oint", 0x222E, OP),
    symbol("omega", 0x03C9, ORD),
    symbol("omicron", 0x03BF, ORD),
    symbol("ominus", 0x2296, BIN),
    symbol("oplus", 0x2295, BIN),
    symbol("oslash", 0x2298, BIN),
    symbol("otimes", 0x2297, BIN),
    symbol("parallel", 0x2225, REL),
    symbol("partial", 0x2202, ORD),
    symbol("perp", 0x27C2, REL),
    symbol("phi", 0x03D5, ORD),
    symbol("pi", 0x03C0, ORD),
    symbol("pm", 0x00B1, BIN),
    symbol("prec", 0x227A, REL),
    symbol("preceq", 0x2AAF, REL),
    symbol("prime", 0x2032, ORD),
    symbol("prod", 0x220F, OP, DISPLAY),
    symbol("propto", 0x221D, REL),
    symbol("psi", 0x03C8, ORD),
    symbol("rVert", 0x2016, CLOSE),
    symbol("rangle", 0x27E9, CLOSE),
    symbol("rbrace", 0x007D, CLOSE),
    symbol("rceil", 0x2309, CLOSE),
    symbol("rfloor", 0x230B, CLOSE),
    symbol("rho", 0x03C1, ORD),
    symbol("rightarrow", 0x2192, REL),
    symbol("rightharpoondown", 0x21C1, REL),
    symbol("rightharpoonup", 0x21C0, REL),
    symbol("rightleftharpoons", 0x21CC, REL),
    symbol("rvert", 0x007C, CLOSE),
    symbol("searrow", 0x2198, REL),
    symbol("setminus", 0x2216, BIN),
    symbol("sharp", 0x266F, ORD),
    symbol("sigma", 0x03C3, ORD),
    symbol("sim", 0x223C, REL),
    symbol("simeq", 0x2243, REL),
    symbol("smile", 0x2323, REL),
    symbol("spadesuit", 0x2660, ORD),
    symbol("sqcap", 0x2293, BIN),
    symbol("sqcup", 0x2294, BIN),
    symbol("sqsubseteq", 0x2291, REL),
    symbol("sqsupseteq", 0x2292, REL),
    symbol("square", 0x25A1, ORD),
    symbol("star", 0x22C6, BIN),
    symbol("subset", 0x2282, REL),
    symbol("subseteq", 0x2286, REL),
    symbol("succ", 0x227B, REL),
    symbol("succeq", 0x2AB0, REL),
    symbol("sum", 0x2211, OP, DISPLAY),
    symbol("supset", 0x2283, REL),
    symbol("supseteq", 0x2287, REL),
    symbol("surd", 0x221A, ORD),
    symbol("swarrow", 0x2199, REL),
    symbol("tau", 0x03C4, ORD),
    symbol("theta", 0x03B8, ORD),
    symbol("times", 0x00D7, BIN),
    symbol("to", 0x2192, REL),
    symbol("top", 0x22A4, ORD),
    symbol("triangle", 0x25B3, ORD),
    symbol("triangleleft", 0x25C3, BIN),
    symbol("triangleq", 0x225C, REL),
    symbol("triangleright", 0x25B9, BIN),
    symbol("uparrow", 0x2191, REL),
    symbol("updownarrow", 0x2195, REL),
    symbol("uplus", 0x228E, BIN),
    symbol("upsilon", 0x03C5, ORD),
    symbol("varepsilon", 0x03B5, ORD),
    symbol("varkappa", 0x03F0, ORD),
    symbol("varnothing", 0x2300, ORD),
    symbol("varphi", 0x03C6, ORD),
    symbol("varpi", 0x03D6, ORD),
    symbol("varrho", 0x03F1, ORD),
    symbol("varsigma", 0x03C2, ORD),
    symbol("vartheta", 0x03D1, ORD),
    symbol("vdash", 0x22A2, REL),
    symbol("vdots", 0x22EE, ORD),
    symbol("vee", 0x2228, BIN),
    symbol("vert", 0x007C, ORD),
    symbol("wedge", 0x2227, BIN),
    symbol("wp", 0x2118, ORD),
    symbol("wr", 0x2240, BIN),
    symbol("xi", 0x03BE, ORD),
    symbol("zeta", 0x03B6, ORD),
    symbol("{", 0x007B, OPEN),
    symbol("|", 0x2016, ORD),
    symbol("}", 0x007D, CLOSE),
};

constexpr std::array kDelimiters = {
    delimiter("Downarrow", 0x21D3),
    delimiter("Uparrow", 0x21D1),
    delimiter("Updownarrow", 0x21D5),
    delimiter("Vert", 0x2016),
    delimiter("backslash", 0x005C),
    delimiter("downarrow", 0x2193),
    delimiter("lVert", 0x2016),
    delimiter("langle", 0x27E8),
    delimiter("lbrace", 0x007B),
    delimiter("lceil", 0x2308),
    delimiter("lfloor", 0x230A),
    delimiter("lvert", 0x007C),
    delimiter("rVert", 0x2016),
    delimiter("rangle", 0x27E9),
    delimiter("rbrace", 0x007D),
    delimiter("rceil", 0x2309),
    delimiter("rfloor", 0x230B),
    delimiter("rvert", 0x007C),
    delimiter("uparrow", 0x2191),
    delimiter("updownarrow", 0x2195),
    delimiter("vert", 0x007C),
    delimiter("{", 0x007B),
    delimiter("|", 0x2016),
    delimiter("}", 0x007D),
};

constexpr std::array kAccents = {
    accent("acute", 0x0301),
    accent("bar", 0x0304),
    accent("breve", 0x0306),
    accent("check", 0x030C),
    accent("dddot", 0x20DB),
    accent("ddot", 0x0308),
    accent("dot", 0x0307),
    accent("grave", 0x0300),
    accent("hat", 0x0302),
    accent("mathring", 0x030A),
    wideAccent("overleftarrow", 0x20D6),
    wideAccent("overleftrightarrow", 0x20E1),
    wideAccent("overrightarrow", 0x20D7),
    accent("tilde", 0x0303),
    accent("vec", 0x20D7),
    wideAccent("widecheck", 0x030C),
    wideAccent("widehat", 0x0302),
    wideAccent("widetilde", 0x0303),
};

constexpr std::array kOperatorNames = {
    function("Pr", "Pr", DISPLAY),
    function("arccos", "arccos", NO_LIMITS),
    function("arcsin", "arcsin", NO_LIMITS),
    function("arctan", "arctan", NO_LIMITS),
    function("arg", "arg", NO_LIMITS),
    function("cos", "cos", NO_LIMITS),
    function("cosh", "cosh", NO_LIMITS),
    function("cot", "cot", NO_LIMITS),
    function("coth", "coth", NO_LIMITS),
    function("csc", "csc", NO_LIMITS),
    function("deg", "deg", NO_LIMITS),
    function("det", "det", DISPLAY),
    function("dim", "dim", NO_LIMITS),
    function("exp", "exp", NO_LIMITS),
    function("gcd", "gcd", DISPLAY),
    function("hom", "hom", NO_LIMITS),
    function("inf", "inf", DISPLAY),
    function("ker", "ker", NO_LIMITS),
    function("lg", "lg", NO_LIMITS),
    function("lim", "lim", DISPLAY),
    function("liminf", "lim inf", DISPLAY),
    function("limsup", "lim sup", DISPLAY),
    function("ln", "ln", NO_LIMITS),
    function("log", "log", NO_LIMITS),
    function("max", "max", DISPLAY),
    function("min", "min", DISPLAY),
    function("sec", "sec", NO_LIMITS),
    function("sin", "sin", NO_LIMITS),
    function("sinh", "sinh", NO_LIMITS),
    function("sup", "sup", DISPLAY),
    function("tan", "tan", NO_LIMITS),
    function("tanh", "tanh", NO_LIMITS),
};

constexpr std::array kAlphabets = {
    font("bm", BOLD_ITALIC),
    font("boldsymbol", BOLD_ITALIC),
    font("mathbb", DOUBLE_STRUCK),
    font("mathbf", BOLD),
    font("mathcal", SCRIPT),
    font("mathfrak", FRAKTUR),
    font("mathit", ITALIC),
    font("mathnormal", DEFAULT),
    font("mathrm", UPRIGHT),
    font("mathscr", SCRIPT),
    font("mathsf", SANS_SERIF),
    font("mathtt", MONOSPACE),
};

// In mu, as amsmath has them: \, is 3, \quad an em (18).
constexpr std::array kSpaces = {
    space(" ", 6.0),
    space("!", -3.0),
    space(",", 3.0),
    space(":", 4.0),
    space(";", 5.0),
    space(">", 4.0),
    space("enspace", 9.0),
    space("medspace", 4.0),
    space("negmedspace", -4.0),
    space("negthickspace", -5.0),
    space("negthinspace", -3.0),
    space("qquad", 36.0),
    space("quad", 18.0),
    space("thickspace", 5.0),
    space("thinspace", 3.0),
};

// A relation and its negation, by the relation's code point.
struct NegationEntry
{
    char32_t relation;
    char32_t negation;
};

constexpr NegationEntry negation(char32_t relation, char32_t negated)
{
    return {.relation = relation, .negation = negated};
}

constexpr std::array kNegations = {
    negation(0x003C, 0x226E),
    negation(0x003D, 0x2260),
    negation(0x003E, 0x226F),
    negation(0x2190, 0x219A),
    negation(0x2192, 0x219B),
    negation(0x2194, 0x21AE),
    negation(0x21D0, 0x21CD),
    negation(0x21D2, 0x21CF),
    negation(0x21D4, 0x21CE),
    negation(0x2203, 0x2204),
    negation(0x2208, 0x2209),
    negation(0x220B, 0x220C),
    negation(0x2223, 0x2224),
    negation(0x2225, 0x2226),
    negation(0x223C, 0x2241),
    negation(0x2243, 0x2244),
    negation(0x2245, 0x2247),
    negation(0x2248, 0x2249),
    negation(0x2261, 0x2262),
    negation(0x2264, 0x2270),
    negation(0x2265, 0x2271),
    negation(0x227A, 0x2280),
    negation(0x227B, 0x2281),
    negation(0x2282, 0x2284),
    negation(0x2283, 0x2285),
    negation(0x2286, 0x2288),
    negation(0x2287, 0x2289),
    negation(0x22A2, 0x22AC),
};
// clang-format on

static_assert(sortedByName(kSymbols));
static_assert(sortedByName(kDelimiters));
static_assert(sortedByName(kAccents));
static_assert(sortedByName(kOperatorNames));
static_assert(sortedByName(kAlphabets));
static_assert(sortedByName(kSpaces));
static_assert(std::ranges::is_sorted(kNegations, {}, &NegationEntry::relation));

// Where an alphabet's letters are in the Mathematical Alphanumeric Symbols block, and the letters
// it has elsewhere (in Letterlike Symbols, from before the block existed: ℝ, ℎ).
struct Hole
{
    char32_t letter;
    char32_t form;
};

struct LatinForms
{
    char32_t              capitals = 0;
    char32_t              smalls   = 0;
    std::span<const Hole> holes;
};

constexpr Hole hole(char32_t letter, char32_t form)
{
    return {.letter = letter, .form = form};
}

constexpr std::array kItalicHoles = {hole(U'h', 0x210E)};
constexpr std::array kScriptHoles = {
    hole(U'B', 0x212C), hole(U'E', 0x2130), hole(U'F', 0x2131), hole(U'H', 0x210B),
    hole(U'I', 0x2110), hole(U'L', 0x2112), hole(U'M', 0x2133), hole(U'R', 0x211B),
    hole(U'e', 0x212F), hole(U'g', 0x210A), hole(U'o', 0x2134),
};
constexpr std::array kFrakturHoles = {
    hole(U'C', 0x212D), hole(U'H', 0x210C), hole(U'I', 0x2111),
    hole(U'R', 0x211C), hole(U'Z', 0x2128),
};
constexpr std::array kDoubleStruckHoles = {
    hole(U'C', 0x2102), hole(U'H', 0x210D), hole(U'N', 0x2115), hole(U'P', 0x2119),
    hole(U'Q', 0x211A), hole(U'R', 0x211D), hole(U'Z', 0x2124),
};

std::optional<char32_t> latinForm(char32_t c, const LatinForms& forms)
{
    for (const Hole& entry : forms.holes)
    {
        if (entry.letter == c)
        {
            return entry.form;
        }
    }
    if (c >= U'A' && c <= U'Z')
    {
        return forms.capitals + (c - U'A');
    }
    if (c >= U'a' && c <= U'z')
    {
        return forms.smalls + (c - U'a');
    }
    return std::nullopt;
}

std::optional<char32_t> latin(char32_t c, MathAlphabet alphabet)
{
    switch (alphabet)
    {
        case MathAlphabet::DEFAULT:
        case MathAlphabet::ITALIC:
            return latinForm(c, {.capitals = 0x1D434, .smalls = 0x1D44E, .holes = kItalicHoles});
        case MathAlphabet::BOLD:
            return latinForm(c, {.capitals = 0x1D400, .smalls = 0x1D41A, .holes = {}});
        case MathAlphabet::BOLD_ITALIC:
            return latinForm(c, {.capitals = 0x1D468, .smalls = 0x1D482, .holes = {}});
        case MathAlphabet::SCRIPT:
            return latinForm(c, {.capitals = 0x1D49C, .smalls = 0x1D4B6, .holes = kScriptHoles});
        case MathAlphabet::FRAKTUR:
            return latinForm(c, {.capitals = 0x1D504, .smalls = 0x1D51E, .holes = kFrakturHoles});
        case MathAlphabet::DOUBLE_STRUCK:
            return latinForm(c,
                             {.capitals = 0x1D538, .smalls = 0x1D552, .holes = kDoubleStruckHoles});
        case MathAlphabet::SANS_SERIF:
            return latinForm(c, {.capitals = 0x1D5A0, .smalls = 0x1D5BA, .holes = {}});
        case MathAlphabet::MONOSPACE:
            return latinForm(c, {.capitals = 0x1D670, .smalls = 0x1D68A, .holes = {}});
        case MathAlphabet::UPRIGHT:
            break;
    }
    return std::nullopt;
}

std::optional<char32_t> digit(char32_t c, MathAlphabet alphabet)
{
    if (c < U'0' || c > U'9')
    {
        return std::nullopt;
    }
    switch (alphabet)
    {
        case MathAlphabet::BOLD:
        case MathAlphabet::BOLD_ITALIC:
            return 0x1D7CE + (c - U'0');
        case MathAlphabet::DOUBLE_STRUCK:
            return 0x1D7D8 + (c - U'0');
        case MathAlphabet::SANS_SERIF:
            return 0x1D7E2 + (c - U'0');
        case MathAlphabet::MONOSPACE:
            return 0x1D7F6 + (c - U'0');
        default:
            return std::nullopt;
    }
}

// The place of a Greek letter in an alphabet's run of 25 capitals (and ∇), or of 25 small letters
// (and ∂ and the variants ϵ ϑ ϰ ϕ ϱ ϖ).
std::optional<char32_t> greekCapital(char32_t c)
{
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2)
    {
        return c - 0x391;
    }
    if (c == 0x3F4)  // ϴ
    {
        return 0x11;
    }
    if (c == 0x2207)  // ∇
    {
        return 0x19;
    }
    return std::nullopt;
}

std::optional<char32_t> greekSmall(char32_t c)
{
    if (c >= 0x3B1 && c <= 0x3C9)
    {
        return c - 0x3B1;
    }
    constexpr std::array<std::pair<char32_t, char32_t>, 7> kOthers = {
        {
            {0x2202, 0x19},
            {0x3F5, 0x1A},
            {0x3D1, 0x1B},
            {0x3F0, 0x1C},
            {0x3D5, 0x1D},
            {0x3F1, 0x1E},
            {0x3D6, 0x1F},
        },
    };
    for (const auto& [letter, index] : kOthers)
    {
        if (letter == c)
        {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<char32_t> greek(char32_t c, MathAlphabet alphabet)
{
    const auto capital = greekCapital(c);
    const auto small   = greekSmall(c);
    if (!capital && !small)
    {
        return std::nullopt;
    }
    switch (alphabet)
    {
        case MathAlphabet::DEFAULT:  // TeX: small letters in italic, capitals upright; ∂ as it is
            if (small && c != 0x2202)
            {
                return 0x1D6FC + *small;
            }
            return std::nullopt;
        case MathAlphabet::ITALIC:
            return capital ? 0x1D6E2 + *capital : 0x1D6FC + *small;
        case MathAlphabet::BOLD:
            return capital ? 0x1D6A8 + *capital : 0x1D6C2 + *small;
        case MathAlphabet::BOLD_ITALIC:  // \boldsymbol: capitals upright, as TeX has them
            return capital ? 0x1D6A8 + *capital : 0x1D736 + *small;
        default:
            return std::nullopt;
    }
}

}  // namespace

std::optional<MathSymbol> findSymbol(std::string_view name)
{
    return find(kSymbols, name);
}

std::optional<char32_t> findDelimiter(char32_t character)
{
    switch (character)
    {
        case U'.':
            return 0;  // none
        case U'<':
            return 0x27E8;
        case U'>':
            return 0x27E9;
        case U'(':
        case U')':
        case U'[':
        case U']':
        case U'|':
        case U'/':
        case 0x2016:
        case 0x27E8:
        case 0x27E9:
        case 0x2308:
        case 0x2309:
        case 0x230A:
        case 0x230B:
            return character;
        default:
            return std::nullopt;
    }
}

std::optional<char32_t> findDelimiter(std::string_view command)
{
    return find(kDelimiters, command);
}

std::optional<MathAccent> findAccent(std::string_view name)
{
    return find(kAccents, name);
}

std::optional<OperatorName> findOperatorName(std::string_view name)
{
    return find(kOperatorNames, name);
}

std::optional<MathAlphabet> findAlphabet(std::string_view name)
{
    return find(kAlphabets, name);
}

std::optional<double> findSpace(std::string_view name)
{
    return find(kSpaces, name);
}

std::optional<char32_t> negated(char32_t relation)
{
    const std::span negations(kNegations);
    const auto found = std::ranges::lower_bound(negations, relation, {}, &NegationEntry::relation);
    if (found == negations.end() || found->relation != relation)
    {
        return std::nullopt;
    }
    return found->negation;
}

AtomType typeOf(char32_t character)
{
    switch (character)
    {
        case U'+':
        case U'-':
        case U'*':
        case 0x2212:  // −
        case 0xB7:    // ·
            return AtomType::BIN;
        case U'=':
        case U'<':
        case U'>':
        case U':':
            return AtomType::REL;
        case U',':
        case U';':
            return AtomType::PUNCT;
        case U'(':
        case U'[':
            return AtomType::OPEN;
        case U')':
        case U']':
        case U'!':
        case U'?':
            return AtomType::CLOSE;
        default:
            break;
    }
    if (character < 0x80)
    {
        return AtomType::ORD;
    }
    // A symbol typed as itself: ≤ is a relation as \leq is.
    const std::span symbols(kSymbols);
    const auto      found = std::ranges::find(
        symbols, character, [](const Named<MathSymbol>& entry) { return entry.value.codePoint; });
    return found != symbols.end() ? found->value.type : AtomType::ORD;
}

char32_t mathCharacter(char32_t character)
{
    switch (character)
    {
        case U'-':
            return 0x2212;
        case U'*':
            return 0x2217;
        default:
            return character;
    }
}

char32_t styledCharacter(char32_t character, MathAlphabet alphabet)
{
    if (const auto form = latin(character, alphabet))
    {
        return *form;
    }
    if (const auto form = digit(character, alphabet))
    {
        return *form;
    }
    if (const auto form = greek(character, alphabet))
    {
        return *form;
    }
    // Dotless i and j, for accents: italic like other letters.
    if (alphabet == MathAlphabet::DEFAULT || alphabet == MathAlphabet::ITALIC)
    {
        if (character == 0x131)
        {
            return 0x1D6A4;
        }
        if (character == 0x237)
        {
            return 0x1D6A5;
        }
    }
    return character;
}

}  // namespace rocketplot::core
