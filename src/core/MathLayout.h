#pragma once

#include "core/MathAtom.h"
#include "core/MathBox.h"
#include "core/MathFont.h"

namespace rocketplot::core
{

/// @p list typeset in @p font as TeX does it (TeXbook Appendix G), with the font's MATH constants
/// where TeX has its own parameters, as LuaTeX and MathML Core use them. In @p style: TEXT for
/// math in a line of text, as between $ signs. In font units.
[[nodiscard]] MathBox layoutMath(const MathList& list, const MathFont& font,
                                 MathStyle style = MathStyle::TEXT);

}  // namespace rocketplot::core
