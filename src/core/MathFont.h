#pragma once

#include <utility>

#include "core/GlyphSubstitutions.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

/// A glyph's advance and the box of its ink, in font units, y up from the baseline.
struct GlyphMetrics
{
    double advance = 0.0;
    double xMin    = 0.0;
    double yMin    = 0.0;
    double xMax    = 0.0;
    double yMax    = 0.0;
};

/// What typesetting needs of an OpenType math font: its glyphs and their metrics (the widget
/// layer has them from Qt; a test makes them up), its MATH table and glyph substitutions.
class MathFont
{
public:
    MathFont(MathTable table, GlyphSubstitutions substitutions, double unitsPerEm)
      : m_table(std::move(table)),
        m_substitutions(std::move(substitutions)),
        m_unitsPerEm(unitsPerEm)
    {
    }
    virtual ~MathFont()                  = default;
    MathFont(const MathFont&)            = delete;
    MathFont& operator=(const MathFont&) = delete;
    MathFont(MathFont&&)                 = delete;
    MathFont& operator=(MathFont&&)      = delete;

    /// The glyph of @p character; 0, the missing glyph, if the font has none.
    [[nodiscard]] virtual GlyphId      glyph(char32_t character) const = 0;
    [[nodiscard]] virtual GlyphMetrics metrics(GlyphId glyph) const    = 0;

    [[nodiscard]] const MathTable&          table() const { return m_table; }
    [[nodiscard]] const GlyphSubstitutions& substitutions() const { return m_substitutions; }
    [[nodiscard]] double                    unitsPerEm() const { return m_unitsPerEm; }

private:
    MathTable          m_table;
    GlyphSubstitutions m_substitutions;
    double             m_unitsPerEm;
};

}  // namespace rocketplot::core
