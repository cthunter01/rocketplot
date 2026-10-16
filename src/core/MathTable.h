#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

/// The constants of an OpenType MATH table, in its order: how a math font wants scripts,
/// fractions, radicals and the like placed, in font units (or percent, where the name says so).
enum class MathConstant : std::uint8_t
{
    SCRIPT_PERCENT_SCALE_DOWN,
    SCRIPT_SCRIPT_PERCENT_SCALE_DOWN,
    DELIMITED_SUB_FORMULA_MIN_HEIGHT,
    DISPLAY_OPERATOR_MIN_HEIGHT,
    MATH_LEADING,
    AXIS_HEIGHT,
    ACCENT_BASE_HEIGHT,
    FLATTENED_ACCENT_BASE_HEIGHT,
    SUBSCRIPT_SHIFT_DOWN,
    SUBSCRIPT_TOP_MAX,
    SUBSCRIPT_BASELINE_DROP_MIN,
    SUPERSCRIPT_SHIFT_UP,
    SUPERSCRIPT_SHIFT_UP_CRAMPED,
    SUPERSCRIPT_BOTTOM_MIN,
    SUPERSCRIPT_BASELINE_DROP_MAX,
    SUB_SUPERSCRIPT_GAP_MIN,
    SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT,
    SPACE_AFTER_SCRIPT,
    UPPER_LIMIT_GAP_MIN,
    UPPER_LIMIT_BASELINE_RISE_MIN,
    LOWER_LIMIT_GAP_MIN,
    LOWER_LIMIT_BASELINE_DROP_MIN,
    STACK_TOP_SHIFT_UP,
    STACK_TOP_DISPLAY_STYLE_SHIFT_UP,
    STACK_BOTTOM_SHIFT_DOWN,
    STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN,
    STACK_GAP_MIN,
    STACK_DISPLAY_STYLE_GAP_MIN,
    STRETCH_STACK_TOP_SHIFT_UP,
    STRETCH_STACK_BOTTOM_SHIFT_DOWN,
    STRETCH_STACK_GAP_ABOVE_MIN,
    STRETCH_STACK_GAP_BELOW_MIN,
    FRACTION_NUMERATOR_SHIFT_UP,
    FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP,
    FRACTION_DENOMINATOR_SHIFT_DOWN,
    FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN,
    FRACTION_NUMERATOR_GAP_MIN,
    FRACTION_NUM_DISPLAY_STYLE_GAP_MIN,
    FRACTION_RULE_THICKNESS,
    FRACTION_DENOMINATOR_GAP_MIN,
    FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN,
    SKEWED_FRACTION_HORIZONTAL_GAP,
    SKEWED_FRACTION_VERTICAL_GAP,
    OVERBAR_VERTICAL_GAP,
    OVERBAR_RULE_THICKNESS,
    OVERBAR_EXTRA_ASCENDER,
    UNDERBAR_VERTICAL_GAP,
    UNDERBAR_RULE_THICKNESS,
    UNDERBAR_EXTRA_DESCENDER,
    RADICAL_VERTICAL_GAP,
    RADICAL_DISPLAY_STYLE_VERTICAL_GAP,
    RADICAL_RULE_THICKNESS,
    RADICAL_EXTRA_ASCENDER,
    RADICAL_KERN_BEFORE_DEGREE,
    RADICAL_KERN_AFTER_DEGREE,
    RADICAL_DEGREE_BOTTOM_RAISE_PERCENT,
};

inline constexpr std::size_t kMathConstantCount = 56;

/// Which way a glyph stretches: delimiters and radicals up, wide accents across.
enum class Stretch : std::uint8_t
{
    VERTICAL,
    HORIZONTAL,
};

/// A larger form of a glyph, and how far it reaches in its direction (font units).
struct GlyphVariant
{
    GlyphId glyph   = 0;
    int     advance = 0;

    friend bool operator==(const GlyphVariant&, const GlyphVariant&) = default;
};

/// A piece of a glyph built from parts: its ends overlap the pieces next to it by up to their
/// connector lengths. An extender can be repeated.
struct GlyphPart
{
    GlyphId glyph          = 0;
    int     startConnector = 0;
    int     endConnector   = 0;
    int     fullAdvance    = 0;
    bool    extender       = false;

    friend bool operator==(const GlyphPart&, const GlyphPart&) = default;
};

/// How a glyph gets larger: by its variants, smallest first, and beyond the largest by an
/// assembly of parts (bottom to top, or left to right), if it has one.
struct GlyphConstruction
{
    // NOLINTBEGIN(readability-redundant-member-init): lets designated initializers leave them out
    std::vector<GlyphVariant> variants{};
    std::vector<GlyphPart>    assembly{};
    // NOLINTEND(readability-redundant-member-init)
    int assemblyItalicsCorrection = 0;
};

/// The OpenType MATH table of a font: its constants, the italic corrections and accent places of
/// its glyphs, and how they stretch. A part of the table that is malformed counts as absent.
class MathTable
{
public:
    /// The table in @p data; nullopt if it isn't a MATH table of version 1.
    [[nodiscard]] static std::optional<MathTable> parse(std::span<const std::byte> data);

    [[nodiscard]] int constant(MathConstant which) const;
    /// How much further right a script goes after @p glyph (a slanted f); 0 if none.
    [[nodiscard]] int italicsCorrection(GlyphId glyph) const;
    /// Where an accent over @p glyph goes, from its origin; nullopt for its middle.
    [[nodiscard]] std::optional<int> topAccentAttachment(GlyphId glyph) const;
    /// Whether @p glyph is an extended shape (a tall delimiter): scripts go by its ink.
    [[nodiscard]] bool isExtendedShape(GlyphId glyph) const;
    /// How @p glyph stretches in @p direction; nullptr if it doesn't.
    [[nodiscard]] const GlyphConstruction* construction(GlyphId glyph, Stretch direction) const;
    /// The least overlap of two parts of an assembly.
    [[nodiscard]] int minConnectorOverlap() const { return m_minConnectorOverlap; }

    // A table made in code (a test's font).
    void setConstant(MathConstant which, int value);
    void setItalicsCorrection(GlyphId glyph, int value);
    void setTopAccentAttachment(GlyphId glyph, int value);
    void setExtendedShape(GlyphId glyph);
    void setConstruction(GlyphId glyph, Stretch direction, GlyphConstruction construction);
    void setMinConnectorOverlap(int value) { m_minConnectorOverlap = value; }

private:
    void readConstants(const OpenTypeReader& reader);
    void readGlyphInfo(const OpenTypeReader& reader);
    void readVariants(const OpenTypeReader& reader);

    std::array<int, kMathConstantCount>            m_constants{};
    std::unordered_map<GlyphId, int>               m_italicsCorrections;
    std::unordered_map<GlyphId, int>               m_topAccentAttachments;
    std::unordered_set<GlyphId>                    m_extendedShapes;
    std::unordered_map<GlyphId, GlyphConstruction> m_vertical;
    std::unordered_map<GlyphId, GlyphConstruction> m_horizontal;
    int                                            m_minConnectorOverlap = 0;
};

}  // namespace rocketplot::core
