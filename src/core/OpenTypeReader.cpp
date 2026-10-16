#include "core/OpenTypeReader.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rocketplot::core
{

namespace
{

// sfnt versions: TrueType outlines (1.0 or 'true') or CFF outlines ('OTTO').
constexpr std::uint32_t kTrueType    = 0x00010000;
constexpr std::uint32_t kAppleTrue   = 0x74727565;
constexpr std::uint32_t kOpenTypeCff = 0x4F54544F;

constexpr std::size_t kTableRecords    = 12;  // after the sfnt header
constexpr std::size_t kTableRecordSize = 16;  // tag, checksum, offset, length

// A font has at most 65536 glyphs, and so a coverage table at most this many.
constexpr std::size_t kMaxCoverage = 65536;

std::optional<std::vector<GlyphId>> readGlyphList(const OpenTypeReader& reader, std::size_t count)
{
    std::vector<GlyphId> glyphs;
    glyphs.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto glyph = reader.uint16(4 + (2 * i));
        if (!glyph)
        {
            return std::nullopt;
        }
        glyphs.push_back(*glyph);
    }
    return glyphs;
}

std::optional<std::vector<GlyphId>> readRanges(const OpenTypeReader& reader, std::size_t count)
{
    std::vector<GlyphId> glyphs;
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t record = 4 + (6 * i);
        const auto        first  = reader.uint16(record);
        const auto        last   = reader.uint16(record + 2);
        const auto        index  = reader.uint16(record + 4);
        if (!first || !last || !index || *last < *first ||
            std::size_t{*index} + (*last - *first) + 1 > kMaxCoverage)
        {
            return std::nullopt;
        }
        glyphs.resize(std::max(glyphs.size(), std::size_t{*index} + (*last - *first) + 1));
        for (std::uint32_t glyph = *first; glyph <= *last; ++glyph)
        {
            glyphs[*index + (glyph - *first)] = glyph;
        }
    }
    return glyphs;
}

}  // namespace

OpenTypeReader::OpenTypeReader(std::span<const std::byte> data) : m_data(data) { }

std::optional<std::uint16_t> OpenTypeReader::uint16(std::size_t offset) const
{
    if (offset > m_data.size() || m_data.size() - offset < 2)
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(m_data[offset]) << 8U) |
                                      std::to_integer<unsigned>(m_data[offset + 1]));
}

std::optional<std::int16_t> OpenTypeReader::int16(std::size_t offset) const
{
    const auto value = uint16(offset);
    if (!value)
    {
        return std::nullopt;
    }
    return std::bit_cast<std::int16_t>(*value);
}

std::optional<std::uint32_t> OpenTypeReader::uint32(std::size_t offset) const
{
    const auto high = uint16(offset);
    const auto low  = uint16(offset + 2);
    if (!high || !low)
    {
        return std::nullopt;
    }
    return (static_cast<std::uint32_t>(*high) << 16U) | *low;
}

OpenTypeReader OpenTypeReader::at(std::size_t offset) const
{
    if (offset > m_data.size())
    {
        return {};
    }
    return OpenTypeReader(m_data.subspan(offset));
}

std::optional<std::vector<GlyphId>> readCoverage(const OpenTypeReader& reader)
{
    const auto format = reader.uint16(0);
    const auto count  = reader.uint16(2);
    if (!format || !count)
    {
        return std::nullopt;
    }
    switch (*format)
    {
        case 1:
            return readGlyphList(reader, *count);
        case 2:
            return readRanges(reader, *count);
        default:
            return std::nullopt;
    }
}

std::span<const std::byte> findTable(std::span<const std::byte> font, std::string_view tag)
{
    if (tag.size() != 4)
    {
        return {};
    }
    std::uint32_t wanted = 0;
    for (const char c : tag)
    {
        wanted = (wanted << 8U) | static_cast<unsigned char>(c);
    }

    const OpenTypeReader reader(font);
    const auto           version = reader.uint32(0);
    const auto           count   = reader.uint16(4);
    if (!version || !count ||
        (*version != kTrueType && *version != kAppleTrue && *version != kOpenTypeCff))
    {
        return {};
    }
    for (std::size_t i = 0; i < *count; ++i)
    {
        const std::size_t record = kTableRecords + (i * kTableRecordSize);
        const auto        name   = reader.uint32(record);
        const auto        offset = reader.uint32(record + 8);
        const auto        length = reader.uint32(record + 12);
        if (!name || !offset || !length)
        {
            return {};
        }
        if (*name == wanted)
        {
            if (*offset > font.size() || font.size() - *offset < *length)
            {
                return {};
            }
            return font.subspan(*offset, *length);
        }
    }
    return {};
}

}  // namespace rocketplot::core
