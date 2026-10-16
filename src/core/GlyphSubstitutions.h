#pragma once

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

/// The glyph substitutions of a math font (its GSUB table) that typesetting uses: the forms of
/// glyphs drawn for scripts (feature ssty: a prime raised and larger, letters with the optical
/// size of 7 and 5 point type) and the dotless i and j that accents go over (feature dtls).
class GlyphSubstitutions
{
public:
    /// The substitutions in @p gsub; none of a part that is malformed.
    [[nodiscard]] static GlyphSubstitutions parse(std::span<const std::byte> gsub);

    /// The form of @p glyph for scripts (@p level 1) or scripts of scripts (2); @p glyph itself if
    /// it has none.
    [[nodiscard]] GlyphId scriptStyle(GlyphId glyph, int level) const;
    /// The dotless form of @p glyph (i, j); @p glyph itself if it has none.
    [[nodiscard]] GlyphId dotless(GlyphId glyph) const;

    // Substitutions made in code (a test's font).
    void setScriptStyle(GlyphId glyph, std::vector<GlyphId> forms);
    void setDotless(GlyphId glyph, GlyphId form);

private:
    std::unordered_map<GlyphId, std::vector<GlyphId>> m_scriptStyles;
    std::unordered_map<GlyphId, GlyphId>              m_dotless;
};

}  // namespace rocketplot::core
