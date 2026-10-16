#pragma once

#include "core/MathBox.h"
#include "core/MathFont.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

/// @p glyph as a box, @p scale times its size: its advance, its ink, and its italic correction and
/// accent place from the MATH table. Lone unless it is an extended shape.
[[nodiscard]] MathBox glyphBox(const MathFont& font, GlyphId glyph, double scale);

/// @p glyph made at least @p size long (font units, after @p scale) in @p direction: its first
/// variant that is long enough, else its assembly of parts, else its largest variant. A variant
/// sits on the baseline as glyphs do; an assembly has its bottom (or its left end) at the origin.
[[nodiscard]] MathBox stretchGlyph(const MathFont& font, GlyphId glyph, double size,
                                   Stretch direction, double scale);

}  // namespace rocketplot::core
