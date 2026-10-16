#include "core/StretchyGlyphs.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "FakeMathFont.h"
#include "core/MathBox.h"
#include "core/MathTable.h"

namespace
{

using rocketplot::core::glyphBox;
using rocketplot::core::MathBox;
using rocketplot::core::Stretch;
using rocketplot::core::stretchGlyph;
using namespace rocketplot::test;

constexpr double kTol = 1e-6;

const FakeMathFont& font()
{
    static const FakeMathFont kFont;
    return kFont;
}

TEST(StretchyGlyphs, AGlyphAsABox)
{
    const MathBox box = glyphBox(font(), U'(', 0.5);
    ASSERT_EQ(box.glyphs.size(), 1U);
    EXPECT_NEAR(box.glyphs[0].scale, 0.5, kTol);
    EXPECT_NEAR(box.width, 200.0, kTol);
    EXPECT_NEAR(box.ascent, 375.0, kTol);
    EXPECT_NEAR(box.depth, 125.0, kTol);
    EXPECT_TRUE(box.lone);
    EXPECT_FALSE(glyphBox(font(), kParenVariant1, 1.0).lone);  // an extended shape
    EXPECT_NEAR(glyphBox(font(), U'\U0001D453', 1.0).italicCorrection, 100.0, kTol);
}

TEST(StretchyGlyphs, TheFirstVariantLongEnough)
{
    EXPECT_EQ(stretchGlyph(font(), U'(', 900.0, Stretch::VERTICAL, 1.0).glyphs[0].glyph, U'(');
    EXPECT_EQ(stretchGlyph(font(), U'(', 1200.0, Stretch::VERTICAL, 1.0).glyphs[0].glyph,
              kParenVariant1);
    EXPECT_EQ(stretchGlyph(font(), U'(', 2000.0, Stretch::VERTICAL, 1.0).glyphs[0].glyph,
              kParenVariant2);
    // Variants are compared at their scale.
    EXPECT_EQ(stretchGlyph(font(), U'(', 700.0, Stretch::VERTICAL, 0.5).glyphs[0].glyph,
              kParenVariant1);
    // Without a construction, the glyph; without an assembly, the largest variant.
    EXPECT_EQ(stretchGlyph(font(), U'x', 5000.0, Stretch::VERTICAL, 1.0).glyphs[0].glyph, U'x');
    const MathBox sum = stretchGlyph(font(), 0x2211, 5000.0, Stretch::VERTICAL, 1.0);
    ASSERT_EQ(sum.glyphs.size(), 1U);
    EXPECT_EQ(sum.glyphs[0].glyph, kSumDisplay);
}

// How far each part of a vertical assembly overlaps the one before it.
std::vector<double> overlaps(const MathBox& box)
{
    std::vector<double> result;
    for (std::size_t i = 1; i < box.glyphs.size(); ++i)
    {
        const double advance = box.glyphs[i - 1].glyph == kParenExtender ? 300.0 : 600.0;
        result.push_back(box.glyphs[i - 1].y + advance - box.glyphs[i].y);
    }
    return result;
}

// A ( made @p size tall from its parts: bottom and top 600 each, extenders 300 (as few as reach
// it), overlapping by 20 at least.
void expectParts(const MathBox& box, double size)
{
    ASSERT_GE(box.glyphs.size(), 3U);
    EXPECT_EQ(box.glyphs.front().glyph, kParenBottom);
    EXPECT_EQ(box.glyphs.back().glyph, kParenTop);
    EXPECT_NEAR(box.glyphs.front().y, 0.0, kTol);
    EXPECT_NEAR(box.ascent, size, 1e-6 * size);
}

// Whether the assembly @p box has as few extenders as reach @p size.
bool fewestExtenders(const MathBox& box, double size)
{
    const auto   extenders = static_cast<double>(box.glyphs.size() - 2);
    const double longest   = 1200.0 + (300.0 * extenders) - (20.0 * (extenders + 1.0));
    return longest >= size && longest - 280.0 < size;
}

TEST(StretchyGlyphs, AnAssemblyAsLongAsWanted)
{
    for (const double size : {2001.0, 2500.0, 3333.0, 10000.0})
    {
        SCOPED_TRACE(size);
        const MathBox box = stretchGlyph(font(), U'(', size, Stretch::VERTICAL, 1.0);
        expectParts(box, size);
        EXPECT_TRUE(fewestExtenders(box, size));
        // At least the font's least overlap, at most as much as the connectors allow.
        const std::vector<double> joints = overlaps(box);
        EXPECT_GE(std::ranges::min(joints), 20.0 - kTol);
        EXPECT_LE(std::ranges::max(joints), 200.0 + kTol);
    }
}

TEST(StretchyGlyphs, AHorizontalAssembly)
{
    const MathBox arrow = stretchGlyph(font(), 0x2192, 1500.0, Stretch::HORIZONTAL, 1.0);
    ASSERT_EQ(arrow.glyphs.size(), 4U);
    EXPECT_NEAR(arrow.width, 1500.0, kTol);
    EXPECT_NEAR(arrow.glyphs[1].x, 500.0 - (100.0 / 3.0), kTol);
    EXPECT_NEAR(arrow.glyphs[3].x, 1000.0, kTol);
    EXPECT_NEAR(arrow.depth, 10.0, kTol);
}

}  // namespace
