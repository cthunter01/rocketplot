#include "core/MathTable.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

namespace
{

// The constants: four 16-bit numbers, then 51 MathValueRecords (a value and the offset of a
// device table, ignored), then one more number.
constexpr std::size_t kValueRecords      = 8;
constexpr std::size_t kValueRecordSize   = 4;
constexpr std::size_t kLastConstant      = kValueRecords + (51 * kValueRecordSize);
constexpr std::size_t kConstantTableSize = kLastConstant + 2;

constexpr std::size_t kGlyphVariantSize = 4;   // glyph, advance
constexpr std::size_t kGlyphPartSize    = 10;  // glyph, start, end, full advance, flags
constexpr unsigned    kExtenderFlag     = 0x0001;

std::size_t constantOffset(std::size_t index)
{
    if (index < 4)
    {
        return 2 * index;
    }
    if (index + 1 == kMathConstantCount)
    {
        return kLastConstant;
    }
    return kValueRecords + ((index - 4) * kValueRecordSize);
}

// The subtable at @p offset from @p reader; empty for a null offset.
OpenTypeReader subtable(const OpenTypeReader& reader, std::optional<std::uint16_t> offset)
{
    return offset && *offset != 0 ? reader.at(*offset) : OpenTypeReader();
}

// A table of MathValueRecords for the glyphs of a coverage: italic corrections, accent places.
std::unordered_map<GlyphId, int> readGlyphValues(const OpenTypeReader& reader)
{
    const auto coverage = readCoverage(subtable(reader, reader.uint16(0)));
    const auto count    = reader.uint16(2);
    if (!coverage || !count || *count > coverage->size())
    {
        return {};
    }
    std::unordered_map<GlyphId, int> values;
    for (std::size_t i = 0; i < *count; ++i)
    {
        const auto value = reader.int16(4 + (i * kValueRecordSize));
        if (!value)
        {
            return {};
        }
        values.emplace((*coverage)[i], *value);
    }
    return values;
}

std::optional<std::vector<GlyphPart>> readAssembly(const OpenTypeReader& reader, int& italics)
{
    const auto correction = reader.int16(0);
    const auto count      = reader.uint16(4);
    if (!correction || !count)
    {
        return std::nullopt;
    }
    std::vector<GlyphPart> parts;
    parts.reserve(*count);
    for (std::size_t i = 0; i < *count; ++i)
    {
        const std::size_t record = 6 + (i * kGlyphPartSize);
        const auto        glyph  = reader.uint16(record);
        const auto        start  = reader.uint16(record + 2);
        const auto        end    = reader.uint16(record + 4);
        const auto        full   = reader.uint16(record + 6);
        const auto        flags  = reader.uint16(record + 8);
        if (!glyph || !start || !end || !full || !flags)
        {
            return std::nullopt;
        }
        parts.push_back({
            .glyph          = *glyph,
            .startConnector = *start,
            .endConnector   = *end,
            .fullAdvance    = *full,
            .extender       = (*flags & kExtenderFlag) != 0,
        });
    }
    italics = *correction;
    return parts;
}

std::optional<GlyphConstruction> readConstruction(const OpenTypeReader& reader)
{
    const auto assemblyOffset = reader.uint16(0);
    const auto count          = reader.uint16(2);
    if (!assemblyOffset || !count)
    {
        return std::nullopt;
    }
    GlyphConstruction construction;
    construction.variants.reserve(*count);
    for (std::size_t i = 0; i < *count; ++i)
    {
        const auto glyph   = reader.uint16(4 + (i * kGlyphVariantSize));
        const auto advance = reader.uint16(6 + (i * kGlyphVariantSize));
        if (!glyph || !advance)
        {
            return std::nullopt;
        }
        construction.variants.push_back({.glyph = *glyph, .advance = *advance});
    }
    if (*assemblyOffset != 0)
    {
        auto parts =
            readAssembly(reader.at(*assemblyOffset), construction.assemblyItalicsCorrection);
        if (parts)
        {
            construction.assembly = std::move(*parts);
        }
    }
    return construction;
}

// The constructions of the glyphs of a coverage, whose offsets start at @p offsets.
std::unordered_map<GlyphId, GlyphConstruction> readConstructions(
    const OpenTypeReader& reader, std::optional<std::uint16_t> coverageOffset, std::size_t count,
    std::size_t offsets)
{
    const auto coverage = readCoverage(subtable(reader, coverageOffset));
    if (!coverage || count > coverage->size())
    {
        return {};
    }
    std::unordered_map<GlyphId, GlyphConstruction> constructions;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto offset = reader.uint16(offsets + (2 * i));
        if (!offset)
        {
            return {};
        }
        if (auto construction = readConstruction(subtable(reader, offset)))
        {
            constructions.emplace((*coverage)[i], std::move(*construction));
        }
    }
    return constructions;
}

}  // namespace

std::optional<MathTable> MathTable::parse(std::span<const std::byte> data)
{
    const OpenTypeReader reader(data);
    const auto           version   = reader.uint16(0);
    const auto           constants = reader.uint16(4);
    const auto           glyphInfo = reader.uint16(6);
    const auto           variants  = reader.uint16(8);
    if (!version || *version != 1 || !constants || !glyphInfo || !variants)
    {
        return std::nullopt;
    }
    MathTable table;
    table.readConstants(subtable(reader, constants));
    table.readGlyphInfo(subtable(reader, glyphInfo));
    table.readVariants(subtable(reader, variants));
    return table;
}

void MathTable::readConstants(const OpenTypeReader& reader)
{
    if (reader.data().size() < kConstantTableSize)
    {
        return;
    }
    for (std::size_t i = 0; i < kMathConstantCount; ++i)
    {
        // The two minimum heights are unsigned, the others signed.
        const std::size_t offset = constantOffset(i);
        m_constants.at(i)        = i == 2 || i == 3 ? int{reader.uint16(offset).value_or(0)}
                                                    : int{reader.int16(offset).value_or(0)};
    }
}

void MathTable::readGlyphInfo(const OpenTypeReader& reader)
{
    m_italicsCorrections   = readGlyphValues(subtable(reader, reader.uint16(0)));
    m_topAccentAttachments = readGlyphValues(subtable(reader, reader.uint16(2)));
    if (const auto shapes = readCoverage(subtable(reader, reader.uint16(4))))
    {
        m_extendedShapes.insert(shapes->begin(), shapes->end());
    }
    // The math kern table (offset 6) isn't read: scripts are placed without it.
}

void MathTable::readVariants(const OpenTypeReader& reader)
{
    const auto overlap         = reader.uint16(0);
    const auto verticalCount   = reader.uint16(6);
    const auto horizontalCount = reader.uint16(8);
    if (!overlap || !verticalCount || !horizontalCount)
    {
        return;
    }
    m_minConnectorOverlap = *overlap;
    m_vertical            = readConstructions(reader, reader.uint16(2), *verticalCount, 10);
    m_horizontal          = readConstructions(reader, reader.uint16(4), *horizontalCount,
                                              10 + (2 * std::size_t{*verticalCount}));
}

int MathTable::constant(MathConstant which) const
{
    return m_constants.at(static_cast<std::size_t>(which));
}

int MathTable::italicsCorrection(GlyphId glyph) const
{
    const auto found = m_italicsCorrections.find(glyph);
    return found != m_italicsCorrections.end() ? found->second : 0;
}

std::optional<int> MathTable::topAccentAttachment(GlyphId glyph) const
{
    const auto found = m_topAccentAttachments.find(glyph);
    return found != m_topAccentAttachments.end() ? std::optional(found->second) : std::nullopt;
}

bool MathTable::isExtendedShape(GlyphId glyph) const
{
    return m_extendedShapes.contains(glyph);
}

const GlyphConstruction* MathTable::construction(GlyphId glyph, Stretch direction) const
{
    const auto& constructions = direction == Stretch::VERTICAL ? m_vertical : m_horizontal;
    const auto  found         = constructions.find(glyph);
    return found != constructions.end() ? &found->second : nullptr;
}

void MathTable::setConstant(MathConstant which, int value)
{
    m_constants.at(static_cast<std::size_t>(which)) = value;
}

void MathTable::setItalicsCorrection(GlyphId glyph, int value)
{
    m_italicsCorrections[glyph] = value;
}

void MathTable::setTopAccentAttachment(GlyphId glyph, int value)
{
    m_topAccentAttachments[glyph] = value;
}

void MathTable::setExtendedShape(GlyphId glyph)
{
    m_extendedShapes.insert(glyph);
}

void MathTable::setConstruction(GlyphId glyph, Stretch direction, GlyphConstruction construction)
{
    (direction == Stretch::VERTICAL ? m_vertical : m_horizontal)[glyph] = std::move(construction);
}

}  // namespace rocketplot::core
