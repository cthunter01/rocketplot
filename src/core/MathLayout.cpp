#include "core/MathLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "core/MathAtom.h"
#include "core/MathBox.h"
#include "core/MathFont.h"
#include "core/MathSymbols.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"
#include "core/StretchyGlyphs.h"
#include "core/Utf8.h"

namespace rocketplot::core
{

namespace
{

// TeX's sizes, in em: the space of an empty delimiter (\nulldelimiterspace), the struts of a
// matrix's rows (0.7 and 0.3 \baselineskip), the space between its columns (2\arraycolsep; a
// \quad in cases), the delimiters of \big to \Bigg, and of a \binom (\delim1, \delim2).
constexpr double                kNullDelimiterSpace = 0.12;
constexpr double                kStrutAscent        = 0.84;
constexpr double                kStrutDepth         = 0.36;
constexpr double                kColumnSeparation   = 1.0;
constexpr std::array<double, 4> kBigSizes           = {0.85, 1.15, 1.45, 1.75};
constexpr double                kDisplayDelimiter   = 2.39;
constexpr double                kTextDelimiter      = 1.01;

// How much of what is between \left and \right its delimiters cover at least: TeX's
// \delimiterfactor (per mille) and \delimitershortfall (em).
constexpr double kDelimiterFactor    = 0.901;
constexpr double kDelimiterShortfall = 0.5;

constexpr double kMuPerEm = 18.0;

// The spaces between atoms, in mu (TeXbook chapter 18): 1 thin (3 mu), 2 medium (4), 3 thick
// (5); negative for one left out in script styles.
constexpr std::array<std::array<int, 8>, 8> kSpacing = {
    {
        // Ord  Op   Bin  Rel  Open Close Punct Inner
        {{0, 1, -2, -3, 0, 0, 0, -1}},      // Ord
        {{1, 1, 0, -3, 0, 0, 0, -1}},       // Op
        {{-2, -2, 0, 0, -2, 0, 0, -2}},     // Bin
        {{-3, -3, 0, 0, -3, 0, 0, -3}},     // Rel
        {{0, 0, 0, 0, 0, 0, 0, 0}},         // Open
        {{0, 1, -2, -3, 0, 0, 0, -1}},      // Close
        {{-1, -1, 0, -1, -1, -1, -1, -1}},  // Punct
        {{-1, 1, -2, -3, -1, 0, -1, -1}},   // Inner
    },
};
constexpr std::array<double, 4> kSpaceMu = {0.0, 3.0, 4.0, 5.0};

constexpr char32_t kSquareRoot = 0x221A;

struct Style
{
    MathStyle size    = MathStyle::TEXT;
    bool      cramped = false;
};

MathStyle smaller(MathStyle size)
{
    return size == MathStyle::DISPLAY || size == MathStyle::TEXT ? MathStyle::SCRIPT
                                                                 : MathStyle::SCRIPT_SCRIPT;
}

MathStyle fractionPart(MathStyle size)
{
    switch (size)
    {
        case MathStyle::DISPLAY:
            return MathStyle::TEXT;
        case MathStyle::TEXT:
            return MathStyle::SCRIPT;
        default:
            return MathStyle::SCRIPT_SCRIPT;
    }
}

Style superscriptOf(Style style)
{
    return {.size = smaller(style.size), .cramped = style.cramped};
}

Style subscriptOf(Style style)
{
    return {.size = smaller(style.size), .cramped = true};
}

Style numeratorOf(Style style)
{
    return {.size = fractionPart(style.size), .cramped = style.cramped};
}

Style denominatorOf(Style style)
{
    return {.size = fractionPart(style.size), .cramped = true};
}

Style crampedOf(Style style)
{
    return {.size = style.size, .cramped = true};
}

// How many levels of scripts @p style is: the glyph forms it takes (ssty).
int scriptLevel(Style style)
{
    switch (style.size)
    {
        case MathStyle::SCRIPT:
            return 1;
        case MathStyle::SCRIPT_SCRIPT:
            return 2;
        default:
            return 0;
    }
}

bool isScriptStyle(Style style)
{
    return style.size == MathStyle::SCRIPT || style.size == MathStyle::SCRIPT_SCRIPT;
}

// The over arrows (\overrightarrow) as the arrows they stretch.
std::optional<char32_t> overArrow(char32_t accent)
{
    switch (accent)
    {
        case 0x20D6:
            return 0x2190;
        case 0x20D7:
            return 0x2192;
        case 0x20E1:
            return 0x2194;
        default:
            return std::nullopt;
    }
}

class Typesetter
{
public:
    explicit Typesetter(const MathFont& font) : m_font(&font), m_table(&font.table()) { }

    MathBox list(const MathList& atoms, Style style)
    {
        std::vector<Item> items;
        items.reserve(atoms.size());
        for (const MathAtom& atom : atoms)
        {
            if (atom.kind == AtomKind::STYLE)
            {
                style.size = atom.style.value_or(style.size);
            }
            else if (atom.kind == AtomKind::SPACE)
            {
                items.push_back(
                    {.box = {.width = atom.space * mu(style)}, .style = style, .space = true});
            }
            else
            {
                items.push_back({.box = layout(atom, style), .type = atom.type, .style = style});
            }
        }
        retypeBinaries(items);
        if (items.size() == 1 && !items.front().space)
        {
            return std::move(items.front().box);
        }
        MathBox                 box;
        double                  x = 0.0;
        std::optional<AtomType> previous;
        for (const Item& item : items)
        {
            if (!item.space)
            {
                x += previous ? space(*previous, item.type, item.style) : 0.0;
                previous = item.type;
            }
            box.add(item.box, x, 0.0);
            x += item.box.width;
        }
        box.width = x;
        return box;
    }

private:
    // An atom laid out, and its class for the spaces around it.
    struct Item
    {
        MathBox  box;
        AtomType type  = AtomType::ORD;
        Style    style = {};
        bool     space = false;  // an explicit space: no class
    };

    // TeX's rules 5 and 6: a binary operation without an operand on its left (or on its right) is
    // an ordinary atom, as in -x and (+).
    static void retypeBinaries(std::vector<Item>& items)
    {
        Item* previous = nullptr;
        for (Item& item : items)
        {
            if (item.space)
            {
                continue;
            }
            if (item.type == AtomType::BIN &&
                (previous == nullptr || previous->type == AtomType::BIN ||
                 previous->type == AtomType::OP || previous->type == AtomType::REL ||
                 previous->type == AtomType::OPEN || previous->type == AtomType::PUNCT))
            {
                item.type = AtomType::ORD;
            }
            if ((item.type == AtomType::REL || item.type == AtomType::CLOSE ||
                 item.type == AtomType::PUNCT) &&
                previous != nullptr && previous->type == AtomType::BIN)
            {
                previous->type = AtomType::ORD;
            }
            previous = &item;
        }
        if (previous != nullptr && previous->type == AtomType::BIN)
        {
            previous->type = AtomType::ORD;
        }
    }

    // --- Sizes

    [[nodiscard]] double scale(Style style) const
    {
        switch (style.size)
        {
            case MathStyle::SCRIPT:
                return percent(MathConstant::SCRIPT_PERCENT_SCALE_DOWN, 70);
            case MathStyle::SCRIPT_SCRIPT:
                return percent(MathConstant::SCRIPT_SCRIPT_PERCENT_SCALE_DOWN, 50);
            default:
                return 1.0;
        }
    }

    // A percentage of the table's, or @p fallback if the font has none.
    [[nodiscard]] double percent(MathConstant which, int fallback) const
    {
        const int value = m_table->constant(which);
        return (value > 0 ? value : fallback) / 100.0;
    }

    [[nodiscard]] double constant(MathConstant which, Style style) const
    {
        return m_table->constant(which) * scale(style);
    }

    [[nodiscard]] double em(Style style) const { return m_font->unitsPerEm() * scale(style); }
    [[nodiscard]] double mu(Style style) const { return em(style) / kMuPerEm; }

    [[nodiscard]] double space(AtomType left, AtomType right, Style style) const
    {
        const int code =
            kSpacing.at(static_cast<std::size_t>(left)).at(static_cast<std::size_t>(right));
        if (code == 0 || (code < 0 && isScriptStyle(style)))
        {
            return 0.0;
        }
        return kSpaceMu.at(static_cast<std::size_t>(std::abs(code))) * mu(style);
    }

    // --- Glyphs

    // The glyph of @p character in @p style: the script form in script styles (ssty), the dotless
    // one under an accent (dtls).
    [[nodiscard]] GlyphId glyphOf(char32_t character, Style style, bool dotless = false) const
    {
        GlyphId glyph = m_font->glyph(character);
        if (dotless)
        {
            glyph = m_font->substitutions().dotless(glyph);
        }
        return m_font->substitutions().scriptStyle(glyph, scriptLevel(style));
    }

    [[nodiscard]] MathBox symbol(const MathAtom& atom, Style style, bool dotless = false) const
    {
        const char32_t styled = styledCharacter(atom.codePoint, atom.alphabet);
        GlyphId        glyph  = glyphOf(styled, style, dotless);
        if (glyph == 0 && styled != atom.codePoint)  // a font without the alphabet: as typed
        {
            glyph = glyphOf(atom.codePoint, style, dotless);
        }
        return glyphBox(*m_font, glyph, scale(style));
    }

    // A delimiter at least @p size tall, centred on the axis; an empty one is a little space.
    [[nodiscard]] MathBox delimiter(char32_t character, double size, Style style) const
    {
        if (character == 0)
        {
            return {.width = kNullDelimiterSpace * em(style)};
        }
        MathBox box =
            stretchGlyph(*m_font, m_font->glyph(character), size, Stretch::VERTICAL, scale(style));
        box.raise(constant(MathConstant::AXIS_HEIGHT, style) - ((box.ascent - box.depth) / 2.0));
        box.lone = false;
        return box;
    }

    // @p inner between delimiters @p size tall.
    [[nodiscard]] MathBox delimited(const MathBox& inner, char32_t left, char32_t right,
                                    double size, Style style) const
    {
        const MathBox opening = delimiter(left, size, style);
        const MathBox closing = delimiter(right, size, style);
        MathBox       box;
        box.add(opening, 0.0, 0.0);
        box.add(inner, opening.width, 0.0);
        box.add(closing, opening.width + inner.width, 0.0);
        return box;
    }

    // How tall \left and \right make their delimiters around @p inner (TeX's rule 19).
    [[nodiscard]] double delimiterSize(const MathBox& inner, Style style) const
    {
        const double axis  = constant(MathConstant::AXIS_HEIGHT, style);
        const double reach = std::max(inner.ascent - axis, inner.depth + axis);
        return std::max(2.0 * reach * kDelimiterFactor,
                        (2.0 * reach) - (kDelimiterShortfall * em(style)));
    }

    // --- Atoms

    MathBox layout(const MathAtom& atom, Style style)
    {
        switch (atom.kind)
        {
            case AtomKind::SYMBOL:
                return atom.type == AtomType::OP ? largeOperator(atom, style)
                                                 : ordinary(atom, style);
            case AtomKind::TEXT:
            {
                MathBox box = text(atom, style);
                if (atom.type == AtomType::OP)
                {
                    return operatorScripts(std::move(box), atom, style);
                }
                const double width = box.width;
                return scripts(std::move(box), atom, style, width, width);
            }
            default:
            {
                MathBox      box   = nucleus(atom, style);
                const double width = box.width;
                return scripts(std::move(box), atom, style, width, width);
            }
        }
    }

    MathBox nucleus(const MathAtom& atom, Style style)
    {
        switch (atom.kind)
        {
            case AtomKind::GROUP:
                return list(atom.body, style);
            case AtomKind::FRACTION:
                return fraction(atom, style);
            case AtomKind::RADICAL:
                return radical(atom, style);
            case AtomKind::ACCENT:
                return accent(atom, style);
            case AtomKind::OVERLINE:
                return overline(atom, style);
            case AtomKind::UNDERLINE:
                return underline(atom, style);
            case AtomKind::DELIMITED:
            {
                const MathBox inner = list(atom.body, style);
                return delimited(inner, atom.left, atom.right, delimiterSize(inner, style), style);
            }
            case AtomKind::BIG_DELIMITER:
                return delimiter(atom.codePoint,
                                 kBigSizes.at(std::clamp<std::size_t>(atom.size, 1, 4) - 1) *
                                     m_font->unitsPerEm(),
                                 style);
            case AtomKind::ARRAY:
                return array(atom, style);
            default:
                return {};
        }
    }

    // A character with its italic correction (TeX's rule 17): after it, unless a subscript
    // follows; then its superscript moves right by it.
    MathBox ordinary(const MathAtom& atom, Style style)
    {
        MathBox      box        = symbol(atom, style);
        const double correction = box.italicCorrection;
        const double advance    = box.width;
        if (atom.subscript.empty())
        {
            box.width += correction;
            return scripts(std::move(box), atom, style, advance + correction, advance + correction);
        }
        return scripts(std::move(box), atom, style, advance + correction, advance);
    }

    // Scripts (TeX's rule 18, with the MATH table's constants and gaps as MathML Core has them).
    MathBox scripts(MathBox base, const MathAtom& atom, Style style, double superscriptX,
                    double subscriptX)
    {
        if (atom.superscript.empty() && atom.subscript.empty())
        {
            return base;
        }
        const MathBox up   = list(atom.superscript, superscriptOf(style));
        const MathBox down = list(atom.subscript, subscriptOf(style));
        double        rise = 0.0;  // the superscript's baseline above the base's
        double        drop = 0.0;  // the subscript's below
        if (!base.lone)
        {
            rise = base.ascent - constant(MathConstant::SUPERSCRIPT_BASELINE_DROP_MAX, style);
            drop = base.depth + constant(MathConstant::SUBSCRIPT_BASELINE_DROP_MIN, style);
        }
        if (atom.superscript.empty())
        {
            drop = std::max({
                drop,
                constant(MathConstant::SUBSCRIPT_SHIFT_DOWN, style),
                down.ascent - constant(MathConstant::SUBSCRIPT_TOP_MAX, style),
            });
        }
        else
        {
            rise = std::max({
                rise,
                constant(style.cramped ? MathConstant::SUPERSCRIPT_SHIFT_UP_CRAMPED
                                       : MathConstant::SUPERSCRIPT_SHIFT_UP,
                         style),
                up.depth + constant(MathConstant::SUPERSCRIPT_BOTTOM_MIN, style),
            });
            if (!atom.subscript.empty())
            {
                drop = std::max(drop, constant(MathConstant::SUBSCRIPT_SHIFT_DOWN, style));
                separate(rise, drop, up, down, style);
            }
        }
        MathBox box = std::move(base);
        if (!atom.superscript.empty())
        {
            box.add(up, superscriptX, rise);
        }
        if (!atom.subscript.empty())
        {
            box.add(down, subscriptX, -drop);
        }
        box.width += constant(MathConstant::SPACE_AFTER_SCRIPT, style);
        box.lone             = false;
        box.italicCorrection = 0.0;
        box.topAccentAttachment.reset();
        return box;
    }

    // Moves a superscript up and its subscript down until the gap between them is wide enough:
    // the superscript first, as long as its bottom stays low enough.
    void separate(double& rise, double& drop, const MathBox& up, const MathBox& down,
                  Style style) const
    {
        const double gap    = (rise - up.depth) - (down.ascent - drop);
        const double wanted = constant(MathConstant::SUB_SUPERSCRIPT_GAP_MIN, style);
        if (gap >= wanted)
        {
            return;
        }
        double       missing = wanted - gap;
        const double room = constant(MathConstant::SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT, style) -
                            (rise - up.depth);
        if (room > 0.0)
        {
            const double raised = std::min(room, missing);
            rise += raised;
            missing -= raised;
        }
        drop += missing;
    }

    // A large operator, centred on the axis; in display style its larger form (TeX's rule 13).
    MathBox largeOperator(const MathAtom& atom, Style style)
    {
        GlyphId glyph = glyphOf(styledCharacter(atom.codePoint, atom.alphabet), style);
        if (style.size == MathStyle::DISPLAY)
        {
            if (const GlyphConstruction* construction =
                    m_table->construction(glyph, Stretch::VERTICAL))
            {
                const double least = constant(MathConstant::DISPLAY_OPERATOR_MIN_HEIGHT, style);
                for (const GlyphVariant& variant : construction->variants)
                {
                    glyph = variant.glyph;
                    if (variant.advance * scale(style) >= least)
                    {
                        break;
                    }
                }
            }
        }
        MathBox box = glyphBox(*m_font, glyph, scale(style));
        box.raise(constant(MathConstant::AXIS_HEIGHT, style) - ((box.ascent - box.depth) / 2.0));
        return operatorScripts(std::move(box), atom, style);
    }

    // An operator's scripts: limits above and below it, or beside it with the subscript moved
    // left by its italic correction (∫).
    MathBox operatorScripts(MathBox box, const MathAtom& atom, Style style)
    {
        const double correction = box.italicCorrection;
        const bool withLimits = atom.limits == Limits::LIMITS || (atom.limits == Limits::DISPLAY &&
                                                                  style.size == MathStyle::DISPLAY);
        box.lone              = false;
        if (withLimits && (!atom.superscript.empty() || !atom.subscript.empty()))
        {
            return limits(box, atom, style, correction);
        }
        const double width = box.width;
        return scripts(std::move(box), atom, style, width, width - correction);
    }

    MathBox limits(const MathBox& base, const MathAtom& atom, Style style, double correction)
    {
        const MathBox above = list(atom.superscript, superscriptOf(style));
        const MathBox below = list(atom.subscript, subscriptOf(style));
        const double  width = std::max({base.width, above.width, below.width});
        MathBox       box;
        box.add(base, (width - base.width) / 2.0, 0.0);
        if (!atom.superscript.empty())
        {
            const double gap = std::max(
                constant(MathConstant::UPPER_LIMIT_GAP_MIN, style),
                constant(MathConstant::UPPER_LIMIT_BASELINE_RISE_MIN, style) - above.depth);
            box.add(above, ((width - above.width) / 2.0) + (correction / 2.0),
                    base.ascent + gap + above.depth);
        }
        if (!atom.subscript.empty())
        {
            const double gap = std::max(
                constant(MathConstant::LOWER_LIMIT_GAP_MIN, style),
                constant(MathConstant::LOWER_LIMIT_BASELINE_DROP_MIN, style) - below.ascent);
            box.add(below, ((width - below.width) / 2.0) - (correction / 2.0),
                    -(base.depth + gap + below.ascent));
        }
        box.width = width;
        return box;
    }

    // \text{...}, \sin: upright characters of the font, spaces as typed (thin in a name).
    [[nodiscard]] MathBox text(const MathAtom& atom, Style style) const
    {
        const MathAlphabet alphabet =
            atom.alphabet == MathAlphabet::DEFAULT ? MathAlphabet::UPRIGHT : atom.alphabet;
        MathBox box;
        double  x = 0.0;
        for (const char32_t c : toUtf32(atom.text))
        {
            if (c == U' ')
            {
                x += atom.type == AtomType::OP ? kSpaceMu.at(1) * mu(style) : wordSpace(style);
                continue;
            }
            const MathBox glyph =
                glyphBox(*m_font, glyphOf(styledCharacter(c, alphabet), style), scale(style));
            box.add(glyph, x, 0.0);
            x += glyph.width;
        }
        box.width = x;
        return box;
    }

    [[nodiscard]] double wordSpace(Style style) const
    {
        const double advance = m_font->metrics(m_font->glyph(U' ')).advance;
        return (advance > 0.0 ? advance : m_font->unitsPerEm() / 3.0) * scale(style);
    }

    // --- Constructions

    // The constants of a fraction, or of a \binom's stack, in display or text style.
    struct Placement
    {
        double up       = 0.0;  // the numerator's baseline above the baseline, at least
        double down     = 0.0;  // the denominator's below, at least
        double upperGap = 0.0;  // between the numerator and the rule, or the denominator
        double lowerGap = 0.0;  // between the rule and the denominator
    };

    [[nodiscard]] Placement placement(bool rule, Style style) const
    {
        using enum MathConstant;
        const bool display = style.size == MathStyle::DISPLAY;
        if (!rule)
        {
            return {
                .up   = constant(display ? STACK_TOP_DISPLAY_STYLE_SHIFT_UP : STACK_TOP_SHIFT_UP,
                                 style),
                .down = constant(
                    display ? STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN : STACK_BOTTOM_SHIFT_DOWN,
                    style),
                .upperGap = constant(display ? STACK_DISPLAY_STYLE_GAP_MIN : STACK_GAP_MIN, style),
            };
        }
        return {
            .up = constant(
                display ? FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP : FRACTION_NUMERATOR_SHIFT_UP,
                style),
            .down     = constant(display ? FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN
                                         : FRACTION_DENOMINATOR_SHIFT_DOWN,
                                 style),
            .upperGap = constant(
                display ? FRACTION_NUM_DISPLAY_STYLE_GAP_MIN : FRACTION_NUMERATOR_GAP_MIN, style),
            .lowerGap = constant(
                display ? FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN : FRACTION_DENOMINATOR_GAP_MIN,
                style),
        };
    }

    // \frac, \dfrac, \binom (TeX's rule 15).
    MathBox fraction(const MathAtom& atom, Style outer)
    {
        const Style style =
            atom.style ? Style{.size = *atom.style, .cramped = outer.cramped} : outer;
        const MathBox numerator   = list(atom.body, numeratorOf(style));
        const MathBox denominator = list(atom.lower, denominatorOf(style));
        const double  axis        = constant(MathConstant::AXIS_HEIGHT, style);
        const double  thickness =
            atom.rule ? constant(MathConstant::FRACTION_RULE_THICKNESS, style) : 0.0;
        Placement place = placement(atom.rule, style);
        if (atom.rule)
        {
            place.up =
                std::max(place.up, axis + (thickness / 2.0) + place.upperGap + numerator.depth);
            place.down = std::max(place.down,
                                  denominator.ascent + place.lowerGap - (axis - (thickness / 2.0)));
        }
        else if (const double gap =
                     (place.up - numerator.depth) - (denominator.ascent - place.down);
                 gap < place.upperGap)
        {
            place.up += (place.upperGap - gap) / 2.0;
            place.down += (place.upperGap - gap) / 2.0;
        }
        const bool   delimiters = atom.left != 0 || atom.right != 0;
        const double padding    = delimiters ? 0.0 : kNullDelimiterSpace * em(style);
        const double width      = std::max(numerator.width, denominator.width);
        MathBox      box;
        box.add(numerator, padding + ((width - numerator.width) / 2.0), place.up);
        box.add(denominator, padding + ((width - denominator.width) / 2.0), -place.down);
        if (atom.rule)
        {
            box.rules.push_back(
                {.x = padding, .y = axis - (thickness / 2.0), .width = width, .height = thickness});
        }
        box.width = width + (2.0 * padding);
        if (!delimiters)
        {
            return box;
        }
        const double size =
            (style.size == MathStyle::DISPLAY ? kDisplayDelimiter : kTextDelimiter) * em(style);
        return delimited(box, atom.left, atom.right, size, style);
    }

    // \sqrt and \sqrt[degree] (TeX's rule 11).
    MathBox radical(const MathAtom& atom, Style style)
    {
        using enum MathConstant;
        const MathBox radicand  = list(atom.body, crampedOf(style));
        const double  thickness = constant(RADICAL_RULE_THICKNESS, style);
        double gap = constant(style.size == MathStyle::DISPLAY ? RADICAL_DISPLAY_STYLE_VERTICAL_GAP
                                                               : RADICAL_VERTICAL_GAP,
                              style);
        const double wanted = radicand.ascent + radicand.depth + gap + thickness;
        MathBox surd = stretchGlyph(*m_font, m_font->glyph(kSquareRoot), wanted, Stretch::VERTICAL,
                                    scale(style));
        if (const double extra = surd.ascent + surd.depth - wanted; extra > 0.0)
        {
            gap += extra / 2.0;  // the room left over, shared above and below the radicand
        }
        const double top = radicand.ascent + gap + thickness;  // of the rule, and the surd
        surd.raise(top - surd.ascent);

        MathBox box;
        double  x = 0.0;
        if (!atom.lower.empty())
        {
            const MathBox degree =
                list(atom.lower, {.size = MathStyle::SCRIPT_SCRIPT, .cramped = true});
            const double before = constant(RADICAL_KERN_BEFORE_DEGREE, style);
            const double raised =
                percent(RADICAL_DEGREE_BOTTOM_RAISE_PERCENT, 60) * (surd.ascent + surd.depth);
            box.add(degree, before, raised - surd.depth);
            x = std::max(0.0, before + degree.width + constant(RADICAL_KERN_AFTER_DEGREE, style));
        }
        box.add(surd, x, 0.0);
        x += surd.width;
        box.add(radicand, x, 0.0);
        box.rules.push_back(
            {.x = x, .y = top - thickness, .width = radicand.width, .height = thickness});
        box.width  = x + radicand.width;
        box.ascent = std::max(box.ascent, top + constant(RADICAL_EXTRA_ASCENDER, style));
        return box;
    }

    // \hat and the like over their base (TeX's rule 12): at the base's accent place, raised as
    // far as the base is taller than the font's accent base height. A wide accent is the largest
    // of its forms as wide as the base at most; an arrow is stretched over it.
    MathBox accent(const MathAtom& atom, Style style)
    {
        const bool loneSymbol =
            atom.body.size() == 1 && atom.body.front().kind == AtomKind::SYMBOL &&
            atom.body.front().type != AtomType::OP && atom.body.front().superscript.empty() &&
            atom.body.front().subscript.empty();
        MathBox base = loneSymbol ? symbol(atom.body.front(), crampedOf(style), true)
                                  : list(atom.body, crampedOf(style));
        if (const auto arrow = atom.wide ? overArrow(atom.codePoint) : std::nullopt)
        {
            return overArrowed(std::move(base), *arrow, style);
        }
        const MathBox mark   = accentMark(atom, base.width, style);
        const double  baseAt = base.topAccentAttachment.value_or(base.width / 2.0);
        const double  raised =
            std::max(0.0, base.ascent - constant(MathConstant::ACCENT_BASE_HEIGHT, style));
        const double width = base.width;
        base.add(mark, baseAt - attachment(mark), raised);
        base.width            = width;
        base.lone             = false;
        base.italicCorrection = 0.0;
        base.topAccentAttachment.reset();
        return base;
    }

    // The glyph of an accent: its own, or for a wide accent its largest form not wider than
    // @p width.
    [[nodiscard]] MathBox accentMark(const MathAtom& atom, double width, Style style) const
    {
        GlyphId glyph = m_font->glyph(atom.codePoint);
        if (const GlyphConstruction* construction =
                m_table->construction(glyph, Stretch::HORIZONTAL);
            atom.wide && construction != nullptr)
        {
            for (const GlyphVariant& variant : construction->variants)
            {
                if (variant.advance * scale(style) > width &&
                    variant.glyph != construction->variants.front().glyph)
                {
                    break;
                }
                glyph = variant.glyph;
            }
        }
        return glyphBox(*m_font, glyph, scale(style));
    }

    // Where an accent's mark attaches: its accent place, or the middle of its ink.
    [[nodiscard]] double attachment(const MathBox& mark) const
    {
        if (mark.topAccentAttachment)
        {
            return *mark.topAccentAttachment;
        }
        if (mark.glyphs.size() == 1)
        {
            const PositionedGlyph& glyph   = mark.glyphs.front();
            const GlyphMetrics     metrics = m_font->metrics(glyph.glyph);
            return (metrics.xMin + metrics.xMax) / 2.0 * glyph.scale;
        }
        return mark.width / 2.0;
    }

    // An arrow over @p base, as wide as it.
    [[nodiscard]] MathBox overArrowed(MathBox base, char32_t arrow, Style style) const
    {
        const MathBox mark  = stretchGlyph(*m_font, m_font->glyph(arrow), base.width,
                                           Stretch::HORIZONTAL, scale(style));
        const double  width = base.width;
        base.add(mark, (width - mark.width) / 2.0,
                 base.ascent + constant(MathConstant::OVERBAR_VERTICAL_GAP, style) + mark.depth);
        base.width            = std::max(width, mark.width);
        base.lone             = false;
        base.italicCorrection = 0.0;
        base.topAccentAttachment.reset();
        return base;
    }

    MathBox overline(const MathAtom& atom, Style style)
    {
        using enum MathConstant;
        MathBox      box       = list(atom.body, crampedOf(style));
        const double thickness = constant(OVERBAR_RULE_THICKNESS, style);
        const double bottom    = box.ascent + constant(OVERBAR_VERTICAL_GAP, style);
        box.rules.push_back({.x = 0.0, .y = bottom, .width = box.width, .height = thickness});
        box.ascent = bottom + thickness + constant(OVERBAR_EXTRA_ASCENDER, style);
        box.lone   = false;
        box.topAccentAttachment.reset();
        return box;
    }

    MathBox underline(const MathAtom& atom, Style style)
    {
        using enum MathConstant;
        MathBox      box       = list(atom.body, style);
        const double thickness = constant(UNDERBAR_RULE_THICKNESS, style);
        const double top       = -(box.depth + constant(UNDERBAR_VERTICAL_GAP, style));
        box.rules.push_back(
            {.x = 0.0, .y = top - thickness, .width = box.width, .height = thickness});
        box.depth = -top + thickness + constant(UNDERBAR_EXTRA_DESCENDER, style);
        box.lone  = false;
        box.topAccentAttachment.reset();
        return box;
    }

    // A matrix or cases: rows of cells in text style, centred on the axis, between delimiters.
    MathBox array(const MathAtom& atom, Style style)
    {
        const Style                       cellStyle{.size = MathStyle::TEXT, .cramped = false};
        std::vector<std::vector<MathBox>> cells;
        std::vector<double>               columns;
        std::vector<double>               ascents;
        std::vector<double>               depths;
        for (const auto& row : atom.cells)
        {
            auto& boxes = cells.emplace_back();
            ascents.push_back(kStrutAscent * em(cellStyle));
            depths.push_back(kStrutDepth * em(cellStyle));
            for (std::size_t column = 0; column < row.size(); ++column)
            {
                const MathBox& cell = boxes.emplace_back(list(row[column], cellStyle));
                columns.resize(std::max(columns.size(), column + 1), 0.0);
                columns[column] = std::max(columns[column], cell.width);
                ascents.back()  = std::max(ascents.back(), cell.ascent);
                depths.back()   = std::max(depths.back(), cell.depth);
            }
        }
        const double separation = kColumnSeparation * em(cellStyle);
        double       height     = 0.0;
        for (std::size_t row = 0; row < cells.size(); ++row)
        {
            height += ascents[row] + depths[row];
        }
        MathBox grid;
        double  y = constant(MathConstant::AXIS_HEIGHT, style) + (height / 2.0);  // the top
        for (std::size_t row = 0; row < cells.size(); ++row)
        {
            y -= ascents[row];
            double x = 0.0;
            for (std::size_t column = 0; column < cells[row].size(); ++column)
            {
                const MathBox& cell = cells[row][column];
                grid.add(cell, atom.leftAligned ? x : x + ((columns[column] - cell.width) / 2.0),
                         y);
                x += columns[column] + separation;
            }
            y -= depths[row];
        }
        double width = 0.0;
        for (const double column : columns)
        {
            width += column;
        }
        grid.width  = width + (separation *
                               static_cast<double>(std::max<std::size_t>(columns.size(), 1) - 1));
        grid.ascent = std::max(grid.ascent, y + height);
        grid.depth  = std::max(grid.depth, -y);
        if (atom.left == 0 && atom.right == 0)
        {
            return grid;
        }
        return delimited(grid, atom.left, atom.right, delimiterSize(grid, style), style);
    }

    const MathFont*  m_font;
    const MathTable* m_table;
};

}  // namespace

MathBox layoutMath(const MathList& list, const MathFont& font, MathStyle style)
{
    return Typesetter(font).list(list, {.size = style, .cramped = false});
}

}  // namespace rocketplot::core
