#include "core/GlyphSubstitutions.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/OpenTypeReader.h"

namespace rocketplot::core
{

namespace
{

constexpr std::uint32_t tag(char a, char b, char c, char d)
{
    return (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U) |
           (static_cast<std::uint32_t>(c) << 8U) | static_cast<std::uint32_t>(d);
}

constexpr std::uint32_t kScriptStyle = tag('s', 's', 't', 'y');
constexpr std::uint32_t kDotless     = tag('d', 't', 'l', 's');

constexpr std::uint16_t kSingleSubstitution    = 1;
constexpr std::uint16_t kAlternateSubstitution = 3;
constexpr std::uint16_t kExtension             = 7;

// The forms kept of a glyph: script and script-script style (a font could list many alternates;
// a malformed one, the same long list for every glyph).
constexpr std::size_t kMaxForms = 4;

using Forms = std::unordered_map<GlyphId, std::vector<GlyphId>>;

OpenTypeReader subtable(const OpenTypeReader& reader, std::size_t offsetAt)
{
    const auto offset = reader.uint16(offsetAt);
    return offset && *offset != 0 ? reader.at(*offset) : OpenTypeReader();
}

// The indexes of the lookups of the features tagged @p wanted, for any script and language.
std::vector<std::uint16_t> lookupsOf(const OpenTypeReader& features, std::uint32_t wanted)
{
    std::vector<std::uint16_t> lookups;
    const std::uint16_t        count = features.uint16(0).value_or(0);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t record = 2 + (6 * i);
        if (features.uint32(record) != wanted)
        {
            continue;
        }
        const OpenTypeReader feature = subtable(features, record + 4);
        const std::uint16_t  indexes = feature.uint16(2).value_or(0);
        for (std::size_t j = 0; j < indexes; ++j)
        {
            const auto index = feature.uint16(4 + (2 * j));
            if (index && std::ranges::find(lookups, *index) == lookups.end())
            {
                lookups.push_back(*index);
            }
        }
    }
    return lookups;
}

void readSingle(const OpenTypeReader& reader, Forms& forms)
{
    const auto format   = reader.uint16(0);
    const auto coverage = readCoverage(subtable(reader, 2));
    if (!format || !coverage)
    {
        return;
    }
    if (*format == 1)
    {
        const std::int16_t delta = reader.int16(4).value_or(0);
        for (const GlyphId glyph : *coverage)
        {
            forms[glyph] = {(glyph + static_cast<GlyphId>(delta)) & 0xFFFFU};
        }
        return;
    }
    const std::size_t count = std::min<std::size_t>(reader.uint16(4).value_or(0), coverage->size());
    for (std::size_t i = 0; i < count; ++i)
    {
        if (const auto form = reader.uint16(6 + (2 * i)))
        {
            forms[(*coverage)[i]] = {*form};
        }
    }
}

void readAlternates(const OpenTypeReader& reader, Forms& forms)
{
    const auto coverage = readCoverage(subtable(reader, 2));
    if (reader.uint16(0) != 1 || !coverage)
    {
        return;
    }
    const std::size_t count = std::min<std::size_t>(reader.uint16(4).value_or(0), coverage->size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const OpenTypeReader set = subtable(reader, 6 + (2 * i));
        const std::size_t    n   = std::min<std::size_t>(set.uint16(0).value_or(0), kMaxForms);
        std::vector<GlyphId> alternates;
        for (std::size_t j = 0; j < n; ++j)
        {
            if (const auto alternate = set.uint16(2 + (2 * j)))
            {
                alternates.push_back(*alternate);
            }
        }
        if (!alternates.empty())
        {
            forms[(*coverage)[i]] = std::move(alternates);
        }
    }
}

void readLookup(const OpenTypeReader& lookup, Forms& forms)
{
    const std::uint16_t type  = lookup.uint16(0).value_or(0);
    const std::uint16_t count = lookup.uint16(4).value_or(0);
    for (std::size_t i = 0; i < count; ++i)
    {
        OpenTypeReader table = subtable(lookup, 6 + (2 * i));
        std::uint16_t  kind  = type;
        if (kind == kExtension)  // a subtable further away, of the type it names
        {
            kind                = table.uint16(2).value_or(0);
            const auto distance = table.uint32(4);
            table               = distance ? table.at(*distance) : OpenTypeReader();
        }
        if (kind == kSingleSubstitution)
        {
            readSingle(table, forms);
        }
        else if (kind == kAlternateSubstitution)
        {
            readAlternates(table, forms);
        }
    }
}

Forms readFeature(const OpenTypeReader& gsub, std::uint32_t feature)
{
    Forms                forms;
    const OpenTypeReader lookups = subtable(gsub, 8);
    const std::uint16_t  count   = lookups.uint16(0).value_or(0);
    for (const std::uint16_t index : lookupsOf(subtable(gsub, 6), feature))
    {
        if (index < count)
        {
            readLookup(subtable(lookups, 2 + (2 * std::size_t{index})), forms);
        }
    }
    return forms;
}

}  // namespace

GlyphSubstitutions GlyphSubstitutions::parse(std::span<const std::byte> gsub)
{
    const OpenTypeReader reader(gsub);
    GlyphSubstitutions   substitutions;
    if (reader.uint16(0) != 1)
    {
        return substitutions;
    }
    substitutions.m_scriptStyles = readFeature(reader, kScriptStyle);
    for (const auto& [glyph, forms] : readFeature(reader, kDotless))
    {
        substitutions.m_dotless.emplace(glyph, forms.front());
    }
    return substitutions;
}

GlyphId GlyphSubstitutions::scriptStyle(GlyphId glyph, int level) const
{
    const auto found = m_scriptStyles.find(glyph);
    if (found == m_scriptStyles.end() || level < 1)
    {
        return glyph;
    }
    const std::vector<GlyphId>& forms = found->second;
    return level >= 2 && forms.size() >= 2 ? forms[1] : forms.front();
}

GlyphId GlyphSubstitutions::dotless(GlyphId glyph) const
{
    const auto found = m_dotless.find(glyph);
    return found != m_dotless.end() ? found->second : glyph;
}

void GlyphSubstitutions::setScriptStyle(GlyphId glyph, std::vector<GlyphId> forms)
{
    if (!forms.empty())
    {
        m_scriptStyles[glyph] = std::move(forms);
    }
}

void GlyphSubstitutions::setDotless(GlyphId glyph, GlyphId form)
{
    m_dotless[glyph] = form;
}

}  // namespace rocketplot::core
