#include "core/GlyphSubstitutions.h"

#include <cstddef>
#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "FontFile.h"
#include "core/OpenTypeReader.h"

namespace
{

using rocketplot::core::findTable;
using rocketplot::core::GlyphSubstitutions;

// Glyphs of Latin Modern Math (fontTools has the same numbers).
constexpr rocketplot::core::GlyphId kPrime     = 2974;  // U+2032, "minute"
constexpr rocketplot::core::GlyphId kLetterX   = 89;
constexpr rocketplot::core::GlyphId kLetterI   = 74;
constexpr rocketplot::core::GlyphId kLetterJ   = 75;
constexpr rocketplot::core::GlyphId kItalicI   = 1304;  // U+1D456
constexpr rocketplot::core::GlyphId kParenLeft = 9;

class GlyphSubstitutionsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_bytes = findTable(m_font, "GSUB");
        ASSERT_FALSE(m_bytes.empty());
        m_substitutions = GlyphSubstitutions::parse(m_bytes);
    }

    std::vector<std::byte>     m_font = rocketplot::test::mathFontFile();
    std::span<const std::byte> m_bytes;
    GlyphSubstitutions         m_substitutions;
};

TEST_F(GlyphSubstitutionsTest, ScriptStyles)
{
    // ssty: the prime raised and larger for a superscript; letters of a smaller optical size.
    EXPECT_EQ(m_substitutions.scriptStyle(kPrime, 1), 2981U);
    EXPECT_EQ(m_substitutions.scriptStyle(kPrime, 2), 2988U);
    EXPECT_EQ(m_substitutions.scriptStyle(kLetterX, 1), 1026U);
    EXPECT_EQ(m_substitutions.scriptStyle(kLetterX, 2), 619U);
    EXPECT_EQ(m_substitutions.scriptStyle(kLetterX, 0), kLetterX);
    EXPECT_EQ(m_substitutions.scriptStyle(kParenLeft, 1), kParenLeft);
}

TEST_F(GlyphSubstitutionsTest, DotlessLetters)
{
    EXPECT_EQ(m_substitutions.dotless(kLetterI), 143U);
    EXPECT_EQ(m_substitutions.dotless(kLetterJ), 180U);
    EXPECT_EQ(m_substitutions.dotless(kItalicI), 1322U);  // U+1D6A4
    EXPECT_EQ(m_substitutions.dotless(kLetterX), kLetterX);
}

TEST_F(GlyphSubstitutionsTest, AMalformedTableIsReadSafely)
{
    EXPECT_EQ(GlyphSubstitutions::parse({}).scriptStyle(kPrime, 1), kPrime);
    for (std::size_t size = 0; size < m_bytes.size(); size += 7)
    {
        (void)GlyphSubstitutions::parse(m_bytes.first(size));
    }
    // NOLINTNEXTLINE(bugprone-random-generator-seed): reproducible test data
    std::mt19937                               generator(11);
    std::uniform_int_distribution<std::size_t> place(0, m_bytes.size() - 1);
    std::uniform_int_distribution<int>         value(0, 255);
    for (int i = 0; i < 1000; ++i)
    {
        std::vector<std::byte> corrupt(m_bytes.begin(), m_bytes.end());
        for (int n = 0; n < 8; ++n)
        {
            corrupt[place(generator)] = static_cast<std::byte>(value(generator));
        }
        (void)GlyphSubstitutions::parse(corrupt);
    }
    SUCCEED();
}

TEST(GlyphSubstitutions, MadeSubstitutions)
{
    GlyphSubstitutions substitutions;
    substitutions.setScriptStyle(1, {2});
    substitutions.setScriptStyle(3, {4, 5});
    substitutions.setDotless(6, 7);
    EXPECT_EQ(substitutions.scriptStyle(1, 1), 2U);
    EXPECT_EQ(substitutions.scriptStyle(1, 2), 2U);  // one form for both levels
    EXPECT_EQ(substitutions.scriptStyle(3, 2), 5U);
    EXPECT_EQ(substitutions.dotless(6), 7U);
    EXPECT_EQ(substitutions.dotless(1), 1U);
}

}  // namespace
