#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

/// A glyph of typeset math: where its origin is, and how much it is scaled (scripts are smaller).
struct PositionedGlyph
{
    GlyphId glyph = 0;
    double  x     = 0.0;
    double  y     = 0.0;
    double  scale = 1.0;
};

/// A rule of typeset math (a fraction bar, the bar of a radical): its bottom left corner and size.
struct MathRule
{
    double x      = 0.0;
    double y      = 0.0;
    double width  = 0.0;
    double height = 0.0;
};

/// Typeset math: glyphs and rules in font units, y up, with the origin on the baseline at the left;
/// its width, and how far it reaches above and below the baseline. What TeX keeps of a lone glyph
/// goes with it, for its scripts and accents.
struct MathBox
{
    // NOLINTBEGIN(readability-redundant-member-init): lets designated initializers leave them out
    std::vector<PositionedGlyph> glyphs{};
    std::vector<MathRule>        rules{};
    std::optional<double>        topAccentAttachment{};  // a lone glyph's: where an accent goes
    // NOLINTEND(readability-redundant-member-init)
    double width            = 0.0;
    double ascent           = 0.0;
    double depth            = 0.0;
    double italicCorrection = 0.0;    // a lone glyph's: how far a script after it moves right
    bool   lone             = false;  // a glyph alone (not an extended shape)

    /// Puts @p other into this box with its origin at (@p x, @p y). The box grows to hold it; its
    /// width reaches at least to @p other's right side.
    void add(const MathBox& other, double x, double y)
    {
        for (const PositionedGlyph& glyph : other.glyphs)
        {
            glyphs.push_back(
                {.glyph = glyph.glyph, .x = glyph.x + x, .y = glyph.y + y, .scale = glyph.scale});
        }
        for (const MathRule& rule : other.rules)
        {
            rules.push_back(
                {.x = rule.x + x, .y = rule.y + y, .width = rule.width, .height = rule.height});
        }
        width  = std::max(width, x + other.width);
        ascent = std::max(ascent, y + other.ascent);
        depth  = std::max(depth, other.depth - y);
    }

    /// Moves everything in the box up by @p dy.
    void raise(double dy)
    {
        for (PositionedGlyph& glyph : glyphs)
        {
            glyph.y += dy;
        }
        for (MathRule& rule : rules)
        {
            rule.y += dy;
        }
        ascent += dy;
        depth -= dy;
    }
};

}  // namespace rocketplot::core
