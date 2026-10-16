#include "core/OpenTypeReader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "FontFile.h"

namespace
{

using rocketplot::core::findTable;
using rocketplot::core::GlyphId;
using rocketplot::core::OpenTypeReader;
using rocketplot::core::readCoverage;

using rocketplot::test::mathFontFile;

template <std::size_t N>
std::array<std::byte, N> bytes(const std::array<unsigned char, N>& values)
{
    std::array<std::byte, N> result{};
    for (std::size_t i = 0; i < N; ++i)
    {
        result.at(i) = static_cast<std::byte>(values.at(i));
    }
    return result;
}

TEST(OpenTypeReader, ReadsBigEndianNumbers)
{
    const auto           data = bytes<6>({0x12, 0x34, 0xFF, 0xFE, 0x00, 0x01});
    const OpenTypeReader reader(data);
    EXPECT_EQ(reader.uint16(0), 0x1234);
    EXPECT_EQ(reader.int16(2), -2);
    EXPECT_EQ(reader.uint32(2), 0xFFFE0001U);
    EXPECT_EQ(reader.at(4).uint16(0), 1);
}

TEST(OpenTypeReader, ReadsNothingPastTheEnd)
{
    const auto           data = bytes<3>({0x12, 0x34, 0x56});
    const OpenTypeReader reader(data);
    EXPECT_EQ(reader.uint16(2), std::nullopt);
    EXPECT_EQ(reader.uint32(0), std::nullopt);
    EXPECT_EQ(reader.uint16(SIZE_MAX), std::nullopt);
    EXPECT_TRUE(reader.at(3).empty());
    EXPECT_TRUE(reader.at(4).empty());
    EXPECT_EQ(reader.at(4).uint16(0), std::nullopt);
}

TEST(OpenTypeReader, ReadsCoverageTables)
{
    // Format 1: a list of glyphs; format 2: ranges of glyphs, with their first coverage index.
    const auto list = bytes<10>({0, 1, 0, 3, 0, 5, 0, 9, 0, 12});
    EXPECT_EQ(readCoverage(OpenTypeReader(list)), (std::vector<GlyphId>{5, 9, 12}));
    const auto ranges = bytes<16>({0, 2, 0, 2, 0, 10, 0, 12, 0, 0, 0, 20, 0, 21, 0, 3});
    EXPECT_EQ(readCoverage(OpenTypeReader(ranges)), (std::vector<GlyphId>{10, 11, 12, 20, 21}));
    // Cut short, backwards, of another format.
    EXPECT_EQ(readCoverage(OpenTypeReader(std::span(list).first(8))), std::nullopt);
    const auto backwards = bytes<10>({0, 2, 0, 1, 0, 12, 0, 10, 0, 0});
    EXPECT_EQ(readCoverage(OpenTypeReader(backwards)), std::nullopt);
    const auto other = bytes<4>({0, 3, 0, 0});
    EXPECT_EQ(readCoverage(OpenTypeReader(other)), std::nullopt);
}

TEST(OpenTypeReader, FindsTheTablesOfAFont)
{
    const std::vector<std::byte> font = mathFontFile();
    ASSERT_GT(font.size(), 700'000U);
    const std::span<const std::byte> math = findTable(font, "MATH");
    ASSERT_FALSE(math.empty());
    EXPECT_EQ(OpenTypeReader(math).uint32(0), 0x00010000U);  // MATH version 1.0
    EXPECT_FALSE(findTable(font, "GSUB").empty());
    EXPECT_TRUE(findTable(font, "glyf").empty());  // CFF outlines
    EXPECT_TRUE(findTable(font, "MAT").empty());
}

TEST(OpenTypeReader, FindsNothingInWhatIsntAFont)
{
    EXPECT_TRUE(findTable({}, "MATH").empty());
    const auto text = bytes<12>({'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'n', 't', '!', '!'});
    EXPECT_TRUE(findTable(text, "MATH").empty());

    // A font cut short: its table records, or a table, end past the data.
    const std::vector<std::byte> font = mathFontFile();
    for (const std::size_t size :
         {std::size_t{6}, std::size_t{20}, std::size_t{200}, std::size_t{4000}})
    {
        EXPECT_TRUE(findTable(std::span(font).first(size), "MATH").empty()) << size;
    }
}

}  // namespace
