#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rocketplot::core
{

/// A glyph of a font, by its index.
using GlyphId = std::uint32_t;

/// Reads the big-endian numbers of OpenType font data. Every read is bounds-checked: one that
/// would go past the end of the data gives nullopt, so a malformed font can't make it read
/// outside it.
class OpenTypeReader
{
public:
    OpenTypeReader() = default;
    explicit OpenTypeReader(std::span<const std::byte> data);

    [[nodiscard]] std::optional<std::uint16_t> uint16(std::size_t offset) const;
    [[nodiscard]] std::optional<std::int16_t>  int16(std::size_t offset) const;
    [[nodiscard]] std::optional<std::uint32_t> uint32(std::size_t offset) const;

    /// The data from @p offset on, such as a subtable; empty if @p offset is past the end.
    [[nodiscard]] OpenTypeReader at(std::size_t offset) const;

    [[nodiscard]] std::span<const std::byte> data() const { return m_data; }
    [[nodiscard]] bool                       empty() const { return m_data.empty(); }

private:
    std::span<const std::byte> m_data;
};

/// The glyphs of an OpenType Coverage table (formats 1 and 2) at @p reader, in the order of their
/// coverage indexes: the glyphs a subtable has records for. Nullopt if it is malformed.
[[nodiscard]] std::optional<std::vector<GlyphId>> readCoverage(const OpenTypeReader& reader);

/// The table @p tag ("MATH", "GSUB") of an OpenType or TrueType font file. Empty if the font has
/// none, or isn't one: a font collection, or malformed data.
[[nodiscard]] std::span<const std::byte> findTable(std::span<const std::byte> font,
                                                   std::string_view           tag);

}  // namespace rocketplot::core
