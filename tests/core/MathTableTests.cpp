#include "core/MathTable.h"

#include <array>
#include <cstddef>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "FontFile.h"
#include "core/OpenTypeReader.h"

namespace
{

using rocketplot::core::findTable;
using rocketplot::core::GlyphConstruction;
using rocketplot::core::GlyphPart;
using rocketplot::core::GlyphVariant;
using rocketplot::core::MathConstant;
using rocketplot::core::MathTable;
using rocketplot::core::Stretch;

// Glyphs of Latin Modern Math (fontTools has the same numbers).
constexpr rocketplot::core::GlyphId kParenLeft    = 9;
constexpr rocketplot::core::GlyphId kLetterX      = 89;
constexpr rocketplot::core::GlyphId kItalicF      = 1301;  // U+1D453
constexpr rocketplot::core::GlyphId kItalicX      = 1319;  // U+1D465
constexpr rocketplot::core::GlyphId kRadical      = 3077;
constexpr rocketplot::core::GlyphId kSummation    = 3060;
constexpr rocketplot::core::GlyphId kIntegral     = 3049;
constexpr rocketplot::core::GlyphId kCircumflex   = 2270;  // U+0302, combining
constexpr rocketplot::core::GlyphId kArrowRight   = 1858;
constexpr rocketplot::core::GlyphId kLargestParen = 2499;

class MathTableTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_bytes = findTable(m_font, "MATH");
        ASSERT_FALSE(m_bytes.empty());
        auto table = MathTable::parse(m_bytes);
        ASSERT_TRUE(table);
        m_table = std::move(table).value_or(MathTable());
    }

    std::vector<std::byte>     m_font = rocketplot::test::mathFontFile();
    std::span<const std::byte> m_bytes;
    MathTable                  m_table;
};

TEST_F(MathTableTest, Constants)
{
    EXPECT_EQ(m_table.constant(MathConstant::SCRIPT_PERCENT_SCALE_DOWN), 70);
    EXPECT_EQ(m_table.constant(MathConstant::SCRIPT_SCRIPT_PERCENT_SCALE_DOWN), 50);
    EXPECT_EQ(m_table.constant(MathConstant::DISPLAY_OPERATOR_MIN_HEIGHT), 1300);
    EXPECT_EQ(m_table.constant(MathConstant::AXIS_HEIGHT), 250);
    EXPECT_EQ(m_table.constant(MathConstant::SUPERSCRIPT_SHIFT_UP), 363);
    EXPECT_EQ(m_table.constant(MathConstant::SPACE_AFTER_SCRIPT), 56);
    EXPECT_EQ(m_table.constant(MathConstant::FRACTION_RULE_THICKNESS), 40);
    EXPECT_EQ(m_table.constant(MathConstant::RADICAL_KERN_AFTER_DEGREE), -556);
    EXPECT_EQ(m_table.constant(MathConstant::RADICAL_DEGREE_BOTTOM_RAISE_PERCENT), 60);
    EXPECT_EQ(m_table.minConnectorOverlap(), 20);
}

TEST_F(MathTableTest, GlyphInfo)
{
    EXPECT_EQ(m_table.italicsCorrection(kItalicF), 90);
    EXPECT_EQ(m_table.italicsCorrection(kIntegral), 332);
    EXPECT_EQ(m_table.italicsCorrection(kItalicX), 0);
    EXPECT_EQ(m_table.topAccentAttachment(kItalicX), 329);
    EXPECT_EQ(m_table.topAccentAttachment(kLetterX), 258);
    EXPECT_EQ(m_table.topAccentAttachment(kParenLeft), std::nullopt);
    EXPECT_TRUE(m_table.isExtendedShape(kLargestParen));
    EXPECT_FALSE(m_table.isExtendedShape(kParenLeft));
}

TEST_F(MathTableTest, VerticalVariantsAndAssemblies)
{
    const GlyphConstruction* paren = m_table.construction(kParenLeft, Stretch::VERTICAL);
    ASSERT_NE(paren, nullptr);
    ASSERT_EQ(paren->variants.size(), 8U);
    EXPECT_EQ(paren->variants.front(), (GlyphVariant{.glyph = kParenLeft, .advance = 997}));
    EXPECT_EQ(paren->variants.back(), (GlyphVariant{.glyph = kLargestParen, .advance = 2991}));
    EXPECT_EQ(paren->assembly,
              (std::vector<GlyphPart>{
                  {.glyph = 2503, .startConnector = 0, .endConnector = 249, .fullAdvance = 1495},
                  {.glyph          = 2504,
                   .startConnector = 498,
                   .endConnector   = 498,
                   .fullAdvance    = 498,
                   .extender       = true},
                  {.glyph = 2505, .startConnector = 249, .endConnector = 0, .fullAdvance = 1495},
              }));

    const GlyphConstruction* radical = m_table.construction(kRadical, Stretch::VERTICAL);
    ASSERT_NE(radical, nullptr);
    EXPECT_EQ(radical->variants.size(), 5U);
    EXPECT_EQ(radical->assembly.size(), 3U);

    const GlyphConstruction* sum = m_table.construction(kSummation, Stretch::VERTICAL);
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->variants, (std::vector<GlyphVariant>{{.glyph = kSummation, .advance = 1001},
                                                        {.glyph = 3074, .advance = 1401}}));
    EXPECT_TRUE(sum->assembly.empty());
    EXPECT_EQ(m_table.construction(kParenLeft, Stretch::HORIZONTAL), nullptr);
    EXPECT_EQ(m_table.construction(kLetterX, Stretch::VERTICAL), nullptr);
}

TEST_F(MathTableTest, HorizontalVariants)
{
    const GlyphConstruction* hat = m_table.construction(kCircumflex, Stretch::HORIZONTAL);
    ASSERT_NE(hat, nullptr);
    EXPECT_EQ(hat->variants.size(), 8U);
    EXPECT_TRUE(hat->assembly.empty());
    const GlyphConstruction* arrow = m_table.construction(kArrowRight, Stretch::HORIZONTAL);
    ASSERT_NE(arrow, nullptr);
    EXPECT_EQ(arrow->assembly.size(), 3U);
}

TEST_F(MathTableTest, WhatIsntAMathTable)
{
    EXPECT_FALSE(MathTable::parse({}));
    std::array<std::byte, 10> header{};  // version 2.0, its subtables all at 0
    header.at(1) = std::byte{2};
    EXPECT_FALSE(MathTable::parse(header));
    header.at(1) = std::byte{1};
    EXPECT_TRUE(MathTable::parse(header));  // version 1: a table, with nothing in it
}

TEST_F(MathTableTest, AMalformedTableIsReadSafely)
{
    // Every length the table might be cut to (a sample), then bytes changed at random: read
    // without reading outside it (the sanitizers would tell).
    for (std::size_t size = 0; size < m_bytes.size(); size += 13)
    {
        (void)MathTable::parse(m_bytes.first(size));
    }
    // NOLINTNEXTLINE(bugprone-random-generator-seed): reproducible test data
    std::mt19937                               generator(7);
    std::uniform_int_distribution<std::size_t> place(0, m_bytes.size() - 1);
    std::uniform_int_distribution<int>         value(0, 255);
    for (int i = 0; i < 1000; ++i)
    {
        std::vector<std::byte> corrupt(m_bytes.begin(), m_bytes.end());
        for (int n = 0; n < 8; ++n)
        {
            corrupt[place(generator)] = static_cast<std::byte>(value(generator));
        }
        (void)MathTable::parse(corrupt);
    }
    SUCCEED();
}

TEST(MathTable, AMadeTable)
{
    MathTable table;
    table.setConstant(MathConstant::AXIS_HEIGHT, 250);
    table.setItalicsCorrection(1, 50);
    table.setTopAccentAttachment(1, 300);
    table.setExtendedShape(2);
    table.setConstruction(2, Stretch::VERTICAL, {.variants = {{.glyph = 3, .advance = 2000}}});
    table.setMinConnectorOverlap(20);
    EXPECT_EQ(table.constant(MathConstant::AXIS_HEIGHT), 250);
    EXPECT_EQ(table.constant(MathConstant::FRACTION_RULE_THICKNESS), 0);
    EXPECT_EQ(table.italicsCorrection(1), 50);
    EXPECT_EQ(table.topAccentAttachment(1), 300);
    EXPECT_TRUE(table.isExtendedShape(2));
    ASSERT_NE(table.construction(2, Stretch::VERTICAL), nullptr);
    EXPECT_EQ(table.construction(2, Stretch::VERTICAL)->variants.size(), 1U);
    EXPECT_EQ(table.minConnectorOverlap(), 20);
}

}  // namespace
