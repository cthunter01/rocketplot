#pragma once

// A math font made up for the typesetter's tests: round numbers, so that where things go can be
// worked out by hand.
//   - 1000 units to the em. A character's glyph is the character itself.
//   - Every glyph is a box 500 wide whose ink goes from 0 to 500 (an x), unless set otherwise.
//   - Its constants are listed in makeTable(); script styles are 70% and 50% as large.

#include <array>
#include <unordered_map>
#include <utility>

#include "core/GlyphSubstitutions.h"
#include "core/MathFont.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot::test
{

// Glyphs that are no character: forms of others.
inline constexpr core::GlyphId kParenVariant1  = 0x200001;  // ( 1500 tall
inline constexpr core::GlyphId kParenVariant2  = 0x200002;  // ( 2000 tall
inline constexpr core::GlyphId kParenBottom    = 0x200003;  // the assembly of (
inline constexpr core::GlyphId kParenExtender  = 0x200004;
inline constexpr core::GlyphId kParenTop       = 0x200005;
inline constexpr core::GlyphId kSumDisplay     = 0x200006;  // ∑ in display style, 1400 tall
inline constexpr core::GlyphId kIntDisplay     = 0x200007;  // ∫ in display style, 2000 tall
inline constexpr core::GlyphId kPrimeScript    = 0x200008;  // ′ for scripts
inline constexpr core::GlyphId kDotlessItalicI = 0x1D6A4;
inline constexpr core::GlyphId kHatWide        = 0x200009;  // ^ 1200 wide
inline constexpr core::GlyphId kArrowLeftEnd   = 0x20000A;  // the assembly of →
inline constexpr core::GlyphId kArrowExtender  = 0x20000B;
inline constexpr core::GlyphId kArrowHead      = 0x20000C;

class FakeMathFont final : public core::MathFont
{
public:
    FakeMathFont() : core::MathFont(makeTable(), makeSubstitutions(), 1000.0)
    {
        using M         = core::GlyphMetrics;
        m_metrics[U'('] = M{.advance = 400, .xMin = 100, .yMin = -250, .xMax = 300, .yMax = 750};
        m_metrics[U')'] = M{.advance = 400, .xMin = 100, .yMin = -250, .xMax = 300, .yMax = 750};
        m_metrics[kParenVariant1] =
            M{.advance = 500, .xMin = 100, .yMin = -500, .xMax = 400, .yMax = 1000};
        m_metrics[kParenVariant2] =
            M{.advance = 600, .xMin = 100, .yMin = -750, .xMax = 500, .yMax = 1250};
        m_metrics[kParenBottom] =
            M{.advance = 600, .xMin = 100, .yMin = 0, .xMax = 500, .yMax = 600};
        m_metrics[kParenExtender] =
            M{.advance = 600, .xMin = 100, .yMin = 0, .xMax = 200, .yMax = 300};
        m_metrics[kParenTop] = M{.advance = 600, .xMin = 100, .yMin = 0, .xMax = 500, .yMax = 600};
        m_metrics[0x221A] = M{.advance = 800, .xMin = 50, .yMin = -500, .xMax = 820, .yMax = 500};
        m_metrics[0x2211] = M{.advance = 1000, .xMin = 50, .yMin = -250, .xMax = 950, .yMax = 750};
        m_metrics[kSumDisplay] =
            M{.advance = 1400, .xMin = 50, .yMin = -450, .xMax = 1350, .yMax = 950};
        m_metrics[0x222B] = M{.advance = 600, .xMin = 50, .yMin = -300, .xMax = 550, .yMax = 800};
        m_metrics[kIntDisplay] =
            M{.advance = 900, .xMin = 50, .yMin = -750, .xMax = 850, .yMax = 1250};
        m_metrics[U'\U0001D434'] =
            M{.advance = 700, .xMin = 0, .yMin = 0, .xMax = 700, .yMax = 700};  // 𝐴
        m_metrics[0x302]    = M{.advance = 0, .xMin = -400, .yMin = 550, .xMax = -100, .yMax = 700};
        m_metrics[kHatWide] = M{.advance = 1200, .xMin = 0, .yMin = 550, .xMax = 1200, .yMax = 700};
        m_metrics[0x2192]   = M{.advance = 1000, .xMin = 0, .yMin = -10, .xMax = 1000, .yMax = 510};
        m_metrics[kArrowLeftEnd] =
            M{.advance = 500, .xMin = 0, .yMin = 230, .xMax = 500, .yMax = 270};
        m_metrics[kArrowExtender] =
            M{.advance = 300, .xMin = 0, .yMin = 230, .xMax = 300, .yMax = 270};
        m_metrics[kArrowHead] = M{.advance = 500, .xMin = 0, .yMin = -10, .xMax = 500, .yMax = 510};
        m_metrics[U' ']       = M{.advance = 333, .xMin = 0, .yMin = 0, .xMax = 0, .yMax = 0};
        m_metrics[0x2212] = M{.advance = 500, .xMin = 50, .yMin = 230, .xMax = 450, .yMax = 270};
    }

    [[nodiscard]] core::GlyphId glyph(char32_t character) const override { return character; }

    [[nodiscard]] core::GlyphMetrics metrics(core::GlyphId glyph) const override
    {
        const auto found = m_metrics.find(glyph);
        return found != m_metrics.end() ? found->second
                                        : core::GlyphMetrics{
                                              .advance = 500,
                                              .xMin    = 0,
                                              .yMin    = 0,
                                              .xMax    = 500,
                                              .yMax    = 500,
                                          };
    }

    void setMetrics(core::GlyphId glyph, core::GlyphMetrics metrics) { m_metrics[glyph] = metrics; }

private:
    static core::MathTable makeTable()
    {
        using enum core::MathConstant;
        core::MathTable table;
        const auto      constants = std::to_array<std::pair<core::MathConstant, int>>({
            {SCRIPT_PERCENT_SCALE_DOWN, 70},
            {SCRIPT_SCRIPT_PERCENT_SCALE_DOWN, 50},
            {DISPLAY_OPERATOR_MIN_HEIGHT, 1300},
            {AXIS_HEIGHT, 250},
            {ACCENT_BASE_HEIGHT, 500},
            {SUBSCRIPT_SHIFT_DOWN, 200},
            {SUBSCRIPT_TOP_MAX, 400},
            {SUBSCRIPT_BASELINE_DROP_MIN, 200},
            {SUPERSCRIPT_SHIFT_UP, 400},
            {SUPERSCRIPT_SHIFT_UP_CRAMPED, 300},
            {SUPERSCRIPT_BOTTOM_MIN, 100},
            {SUPERSCRIPT_BASELINE_DROP_MAX, 250},
            {SUB_SUPERSCRIPT_GAP_MIN, 400},
            {SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT, 450},
            {SPACE_AFTER_SCRIPT, 0},
            {UPPER_LIMIT_GAP_MIN, 150},
            {UPPER_LIMIT_BASELINE_RISE_MIN, 300},
            {LOWER_LIMIT_GAP_MIN, 150},
            {LOWER_LIMIT_BASELINE_DROP_MIN, 600},
            {STACK_TOP_SHIFT_UP, 450},
            {STACK_TOP_DISPLAY_STYLE_SHIFT_UP, 700},
            {STACK_BOTTOM_SHIFT_DOWN, 350},
            {STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN, 700},
            {STACK_GAP_MIN, 120},
            {STACK_DISPLAY_STYLE_GAP_MIN, 300},
            {FRACTION_NUMERATOR_SHIFT_UP, 400},
            {FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP, 700},
            {FRACTION_DENOMINATOR_SHIFT_DOWN, 350},
            {FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN, 700},
            {FRACTION_NUMERATOR_GAP_MIN, 50},
            {FRACTION_NUM_DISPLAY_STYLE_GAP_MIN, 150},
            {FRACTION_RULE_THICKNESS, 40},
            {FRACTION_DENOMINATOR_GAP_MIN, 50},
            {FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN, 150},
            {OVERBAR_VERTICAL_GAP, 100},
            {OVERBAR_RULE_THICKNESS, 40},
            {OVERBAR_EXTRA_ASCENDER, 40},
            {UNDERBAR_VERTICAL_GAP, 100},
            {UNDERBAR_RULE_THICKNESS, 40},
            {UNDERBAR_EXTRA_DESCENDER, 40},
            {RADICAL_VERTICAL_GAP, 50},
            {RADICAL_DISPLAY_STYLE_VERTICAL_GAP, 150},
            {RADICAL_RULE_THICKNESS, 40},
            {RADICAL_EXTRA_ASCENDER, 40},
            {RADICAL_KERN_BEFORE_DEGREE, 300},
            {RADICAL_KERN_AFTER_DEGREE, -500},
            {RADICAL_DEGREE_BOTTOM_RAISE_PERCENT, 60},
        });
        for (const auto& [which, value] : constants)
        {
            table.setConstant(which, value);
        }
        table.setMinConnectorOverlap(20);
        table.setItalicsCorrection(U'\U0001D453', 100);    // 𝑓
        table.setItalicsCorrection(0x222B, 200);           // ∫
        table.setTopAccentAttachment(U'\U0001D434', 450);  // 𝐴: right of its middle, as italics are
        table.setExtendedShape(kParenVariant1);
        table.setExtendedShape(kParenVariant2);
        table.setConstruction(U'(', core::Stretch::VERTICAL,
                              {
                                  .variants =
                                      {
                                          {.glyph = U'(', .advance = 1000},
                                          {.glyph = kParenVariant1, .advance = 1500},
                                          {.glyph = kParenVariant2, .advance = 2000},
                                      },
                                  .assembly =
                                      {
                                          {
                                              .glyph          = kParenBottom,
                                              .startConnector = 0,
                                              .endConnector   = 200,
                                              .fullAdvance    = 600,
                                          },
                                          {
                                              .glyph          = kParenExtender,
                                              .startConnector = 300,
                                              .endConnector   = 300,
                                              .fullAdvance    = 300,
                                              .extender       = true,
                                          },
                                          {
                                              .glyph          = kParenTop,
                                              .startConnector = 200,
                                              .endConnector   = 0,
                                              .fullAdvance    = 600,
                                          },
                                      },
                              });
        table.setConstruction(0x221A, core::Stretch::VERTICAL,
                              {.variants = {{.glyph = 0x221A, .advance = 1000}}});
        table.setConstruction(0x2211, core::Stretch::VERTICAL,
                              {
                                  .variants =
                                      {
                                          {.glyph = 0x2211, .advance = 1000},
                                          {.glyph = kSumDisplay, .advance = 1400},
                                      },
                              });
        table.setConstruction(0x222B, core::Stretch::VERTICAL,
                              {
                                  .variants =
                                      {
                                          {.glyph = 0x222B, .advance = 1100},
                                          {.glyph = kIntDisplay, .advance = 2000},
                                      },
                              });
        table.setConstruction(
            0x302, core::Stretch::HORIZONTAL,
            {.variants = {{.glyph = 0x302, .advance = 300}, {.glyph = kHatWide, .advance = 1200}}});
        table.setConstruction(0x2192, core::Stretch::HORIZONTAL,
                              {
                                  .variants = {{.glyph = 0x2192, .advance = 1000}},
                                  .assembly =
                                      {
                                          {
                                              .glyph          = kArrowLeftEnd,
                                              .startConnector = 0,
                                              .endConnector   = 150,
                                              .fullAdvance    = 500,
                                          },
                                          {
                                              .glyph          = kArrowExtender,
                                              .startConnector = 150,
                                              .endConnector   = 150,
                                              .fullAdvance    = 300,
                                              .extender       = true,
                                          },
                                          {
                                              .glyph          = kArrowHead,
                                              .startConnector = 150,
                                              .endConnector   = 0,
                                              .fullAdvance    = 500,
                                          },
                                      },
                              });
        return table;
    }

    static core::GlyphSubstitutions makeSubstitutions()
    {
        core::GlyphSubstitutions substitutions;
        substitutions.setScriptStyle(0x2032, {kPrimeScript});
        substitutions.setDotless(U'\U0001D456', kDotlessItalicI);
        return substitutions;
    }

    std::unordered_map<core::GlyphId, core::GlyphMetrics> m_metrics;
};

}  // namespace rocketplot::test
