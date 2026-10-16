#include "core/StretchyGlyphs.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

#include "core/MathBox.h"
#include "core/MathFont.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

namespace
{

// How often an extender may repeat: plenty for anything a label holds, and a bound on a
// malformed font's.
constexpr int kMaxRepeats = 1000;

// The parts of an assembly with each extender repeated @p repeats times.
std::vector<GlyphPart> expanded(const std::vector<GlyphPart>& parts, int repeats)
{
    std::vector<GlyphPart> sequence;
    for (const GlyphPart& part : parts)
    {
        for (int i = 0; i < (part.extender ? repeats : 1); ++i)
        {
            sequence.push_back(part);
        }
    }
    return sequence;
}

double longest(const std::vector<GlyphPart>& sequence, double overlap, double scale)
{
    double length = 0.0;
    for (const GlyphPart& part : sequence)
    {
        length += part.fullAdvance * scale;
    }
    const double joints = sequence.empty() ? 0.0 : static_cast<double>(sequence.size() - 1);
    return length - (overlap * joints);
}

// An assembly at least @p size long, if its parts reach that far: as few extenders as reach it,
// and one overlap for every joint (as the OpenType specification and MathML Core have it).
MathBox assemble(const MathFont& font, const std::vector<GlyphPart>& parts, double size,
                 Stretch direction, double scale)
{
    const double           minOverlap = font.table().minConnectorOverlap() * scale;
    const bool             extends    = std::ranges::any_of(parts, &GlyphPart::extender);
    std::vector<GlyphPart> sequence   = expanded(parts, 0);
    for (int repeats = 1;
         extends && repeats <= kMaxRepeats && longest(sequence, minOverlap, scale) < size;
         ++repeats)
    {
        sequence = expanded(parts, repeats);
    }

    // The overlap that makes it as long as wanted, within what the connectors allow.
    double total      = 0.0;
    double maxOverlap = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < sequence.size(); ++i)
    {
        total += sequence[i].fullAdvance * scale;
        if (i + 1 < sequence.size())
        {
            maxOverlap = std::min(
                maxOverlap,
                std::min(sequence[i].endConnector, sequence[i + 1].startConnector) * scale);
        }
    }
    const double joints  = static_cast<double>(sequence.size()) - 1.0;
    const double overlap = joints > 0.0 ? std::clamp((total - size) / joints, minOverlap,
                                                     std::max(minOverlap, maxOverlap))
                                        : 0.0;

    MathBox box;
    double  offset = 0.0;
    for (const GlyphPart& part : sequence)
    {
        const MathBox piece = glyphBox(font, part.glyph, scale);
        box.add(piece, direction == Stretch::HORIZONTAL ? offset : 0.0,
                direction == Stretch::VERTICAL ? offset : 0.0);
        offset += (part.fullAdvance * scale) - overlap;
    }
    const double length = offset + overlap;
    if (direction == Stretch::VERTICAL)
    {
        box.ascent = std::max(box.ascent, length);
    }
    else
    {
        box.width = length;
    }
    return box;
}

}  // namespace

MathBox glyphBox(const MathFont& font, GlyphId glyph, double scale)
{
    const GlyphMetrics metrics = font.metrics(glyph);
    const MathTable&   table   = font.table();
    MathBox            box;
    box.glyphs.push_back({.glyph = glyph, .x = 0.0, .y = 0.0, .scale = scale});
    box.width            = metrics.advance * scale;
    box.ascent           = std::max(0.0, metrics.yMax * scale);
    box.depth            = std::max(0.0, -metrics.yMin * scale);
    box.italicCorrection = table.italicsCorrection(glyph) * scale;
    if (const auto attachment = table.topAccentAttachment(glyph))
    {
        box.topAccentAttachment = *attachment * scale;
    }
    box.lone = !table.isExtendedShape(glyph);
    return box;
}

MathBox stretchGlyph(const MathFont& font, GlyphId glyph, double size, Stretch direction,
                     double scale)
{
    const GlyphConstruction* construction = font.table().construction(glyph, direction);
    if (construction == nullptr)
    {
        return glyphBox(font, glyph, scale);
    }
    for (const GlyphVariant& variant : construction->variants)
    {
        if (variant.advance * scale >= size)
        {
            return glyphBox(font, variant.glyph, scale);
        }
    }
    if (!construction->assembly.empty())
    {
        MathBox box          = assemble(font, construction->assembly, size, direction, scale);
        box.italicCorrection = construction->assemblyItalicsCorrection * scale;
        return box;
    }
    return glyphBox(
        font, construction->variants.empty() ? glyph : construction->variants.back().glyph, scale);
}

}  // namespace rocketplot::core
