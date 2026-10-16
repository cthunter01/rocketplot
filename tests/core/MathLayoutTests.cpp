#include "core/MathLayout.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "FakeMathFont.h"
#include "core/MathAtom.h"
#include "core/MathBox.h"
#include "core/MathParser.h"
#include "core/OpenTypeReader.h"

// The positions expected here are worked out by hand from TeX's rules (TeXbook Appendix G) and
// the made-up font's constants (FakeMathFont.h): 1000 units to the em, so a mu is 1000/18.

namespace
{

using rocketplot::core::GlyphId;
using rocketplot::core::layoutMath;
using rocketplot::core::MathBox;
using rocketplot::core::MathStyle;
using rocketplot::core::parseMath;
using rocketplot::core::PositionedGlyph;
using namespace rocketplot::test;

constexpr double kMu  = 1000.0 / 18.0;
constexpr double kTol = 1e-6;

const FakeMathFont& font()
{
    static const FakeMathFont kFont;
    return kFont;
}

MathBox typeset(std::string_view tex, MathStyle style = MathStyle::TEXT)
{
    const auto list = parseMath(tex);
    EXPECT_TRUE(list) << tex;
    return list ? layoutMath(*list, font(), style) : MathBox{};
}

// The @p nth glyph @p glyph of @p box (there is one, or the test fails).
PositionedGlyph find(const MathBox& box, GlyphId glyph, int nth = 0)
{
    for (const PositionedGlyph& placed : box.glyphs)
    {
        if (placed.glyph == glyph && nth-- == 0)
        {
            return placed;
        }
    }
    ADD_FAILURE() << "no glyph " << glyph;
    return {};
}

std::size_t count(const MathBox& box, GlyphId glyph)
{
    return static_cast<std::size_t>(std::ranges::count(box.glyphs, glyph, &PositionedGlyph::glyph));
}

constexpr GlyphId kA = U'\U0001D44E';  // math italic letters, which a, b, x... are
constexpr GlyphId kB = U'\U0001D44F';
constexpr GlyphId kI = U'\U0001D456';
constexpr GlyphId kN = U'\U0001D45B';
constexpr GlyphId kX = U'\U0001D465';

TEST(MathLayout, SpacesBetweenAtoms)
{
    // Ord Bin Ord: medium spaces (4 mu).
    const MathBox sum = typeset("a+b");
    EXPECT_NEAR(find(sum, kA).x, 0.0, kTol);
    EXPECT_NEAR(find(sum, U'+').x, 500.0 + (4 * kMu), kTol);
    EXPECT_NEAR(find(sum, kB).x, 1000.0 + (8 * kMu), kTol);
    EXPECT_NEAR(sum.width, 1500.0 + (8 * kMu), kTol);
    // Ord Rel Ord: thick (5 mu).
    const MathBox equation = typeset("a=b");
    EXPECT_NEAR(find(equation, kB).x, 1000.0 + (10 * kMu), kTol);
    // A binary operation without a left operand is ordinary: no spaces.
    const MathBox negative = typeset("-a");
    EXPECT_NEAR(find(negative, 0x2212).x, 0.0, kTol);
    EXPECT_NEAR(find(negative, kA).x, 500.0, kTol);
    // An operator's name and its argument: a thin space.
    const MathBox sine = typeset(R"(\sin x)");
    EXPECT_NEAR(find(sine, U'n').x, 1000.0, kTol);  // upright
    EXPECT_NEAR(find(sine, kX).x, 1500.0 + (3 * kMu), kTol);
}

TEST(MathLayout, ScriptStylesLeaveOutSomeSpaces)
{
    // In a superscript (script style, 70%) a + and = get no spaces.
    const MathBox power = typeset("x^{a+b=a}");
    const double  start = find(power, kA).x;
    EXPECT_NEAR(start, 500.0, kTol);
    EXPECT_NEAR(find(power, U'+').x, start + 350.0, kTol);
    EXPECT_NEAR(find(power, kB).x, start + 700.0, kTol);
    EXPECT_NEAR(find(power, U'=').x, start + 1050.0, kTol);
    EXPECT_NEAR(find(power, kA).scale, 0.7, kTol);
}

TEST(MathLayout, ExplicitSpaces)
{
    const MathBox spaced = typeset(R"(a\,b\quad a)");
    EXPECT_NEAR(find(spaced, kB).x, 500.0 + (3 * kMu), kTol);
    EXPECT_NEAR(find(spaced, kA, 1).x, 1000.0 + (21 * kMu), kTol);
}

TEST(MathLayout, Superscripts)
{
    const MathBox power = typeset("x^2");
    EXPECT_NEAR(find(power, U'2').x, 500.0, kTol);
    EXPECT_NEAR(find(power, U'2').y, 400.0, kTol);  // SuperscriptShiftUp
    EXPECT_NEAR(find(power, U'2').scale, 0.7, kTol);
    // In a denominator (cramped) a superscript rises less: SuperscriptShiftUpCramped, at 70%.
    const MathBox fraction = typeset(R"(\frac{1}{x^2})");
    EXPECT_NEAR(find(fraction, U'2').y - find(fraction, kX).y, 300.0 * 0.7, kTol);
}

TEST(MathLayout, SubscriptsAndSuperscriptsKeepTheirDistance)
{
    // Shifts 400 up and 200 down leave a gap of 250 between a superscript and subscript 350
    // tall; the font wants 400: the superscript goes up 50 (as far as its bottom may), the
    // subscript down the other 100.
    const MathBox both = typeset("x_i^2");
    EXPECT_NEAR(find(both, U'2').y, 450.0, kTol);
    EXPECT_NEAR(find(both, kI).y, -300.0, kTol);
    EXPECT_NEAR(find(both, kI).x, 500.0, kTol);
    const MathBox subscript = typeset("x_i");
    EXPECT_NEAR(find(subscript, kI).y, -200.0, kTol);  // SubscriptShiftDown
}

TEST(MathLayout, ItalicCorrection)
{
    // After an italic f, its italic correction (100): the superscript goes beyond it; without
    // a subscript the f takes the room; with one, the subscript stays tucked under it.
    EXPECT_NEAR(find(typeset("f^2"), U'2').x, 600.0, kTol);
    const MathBox both = typeset("f_i^2");
    EXPECT_NEAR(find(both, U'2').x, 600.0, kTol);
    EXPECT_NEAR(find(both, kI).x, 500.0, kTol);
    EXPECT_NEAR(find(typeset("f(x)"), U'(').x, 600.0, kTol);
}

TEST(MathLayout, Primes)
{
    // A prime in a superscript is its script form (ssty), which is drawn high already.
    const MathBox prime = typeset("f'");
    EXPECT_EQ(count(prime, 0x2032), 0U);
    EXPECT_NEAR(find(prime, kPrimeScript).scale, 0.7, kTol);
}

TEST(MathLayout, Fractions)
{
    // Text style: the numerator 400 up, the denominator 350 down (their least shifts); a rule
    // 40 thick on the axis (250); a null delimiter's space (120) on either side.
    const MathBox fraction = typeset(R"(\frac{a}{b})");
    EXPECT_NEAR(find(fraction, kA).y, 400.0, kTol);
    EXPECT_NEAR(find(fraction, kA).x, 120.0, kTol);
    EXPECT_NEAR(find(fraction, kB).y, -350.0, kTol);
    ASSERT_EQ(fraction.rules.size(), 1U);
    EXPECT_NEAR(fraction.rules[0].y, 230.0, kTol);
    EXPECT_NEAR(fraction.rules[0].height, 40.0, kTol);
    EXPECT_NEAR(fraction.rules[0].width, 350.0, kTol);
    EXPECT_NEAR(fraction.width, 590.0, kTol);
    // Display style: larger parts and shifts.
    const MathBox display = typeset(R"(\dfrac{a}{b})");
    EXPECT_NEAR(find(display, kA).y, 700.0, kTol);
    EXPECT_NEAR(find(display, kA).scale, 1.0, kTol);
    EXPECT_NEAR(find(display, kB).y, -700.0, kTol);
}

TEST(MathLayout, Binomials)
{
    // No rule; delimiters 1.01 em tall at least: the first form of ( that tall.
    const MathBox binomial = typeset(R"(\binom{n}{k})");
    EXPECT_TRUE(binomial.rules.empty());
    EXPECT_EQ(count(binomial, kParenVariant1), 1U);
    EXPECT_NEAR(find(binomial, kN).y, 450.0, kTol);  // StackTopShiftUp
}

TEST(MathLayout, Radicals)
{
    // The radicand (500 tall) wants a surd 590 tall; the surd is 1000: the gap above the
    // radicand grows by half the difference, to 255, and the rule's top is at 795.
    const MathBox root = typeset(R"(\sqrt{x})");
    EXPECT_NEAR(find(root, 0x221A).y, 295.0, kTol);
    EXPECT_NEAR(find(root, kX).x, 800.0, kTol);
    ASSERT_EQ(root.rules.size(), 1U);
    EXPECT_NEAR(root.rules[0].x, 800.0, kTol);
    EXPECT_NEAR(root.rules[0].y, 755.0, kTol);
    EXPECT_NEAR(root.rules[0].width, 500.0, kTol);
    EXPECT_NEAR(root.ascent, 835.0, kTol);  // with the extra ascender
    // The degree, in script-script style, raised 60% of the surd from its bottom.
    const MathBox cube = typeset(R"(\sqrt[3]{x})");
    EXPECT_NEAR(find(cube, U'3').x, 300.0, kTol);
    EXPECT_NEAR(find(cube, U'3').y, 395.0, kTol);
    EXPECT_NEAR(find(cube, U'3').scale, 0.5, kTol);
    EXPECT_NEAR(find(cube, 0x221A).x, 50.0, kTol);
}

TEST(MathLayout, LargeOperators)
{
    EXPECT_EQ(count(typeset(R"(\sum)"), 0x2211), 1U);
    EXPECT_EQ(count(typeset(R"(\sum)", MathStyle::DISPLAY), kSumDisplay),
              1U);  // 1300 tall at least
    // Limits in display style, beside it in text style.
    const MathBox display = typeset(R"(\sum_i^n)", MathStyle::DISPLAY);
    EXPECT_NEAR(find(display, kN).y, 950.0 + 300.0, kTol);
    EXPECT_NEAR(find(display, kN).x, (1400.0 - 350.0) / 2.0, kTol);
    EXPECT_NEAR(find(display, kI).y, -(450.0 + 250.0 + 350.0), kTol);
    const MathBox text = typeset(R"(\sum_i^n)");
    EXPECT_NEAR(find(text, kN).x, 1000.0, kTol);
    EXPECT_NEAR(find(text, kN).y, 750.0 - 250.0, kTol);
    EXPECT_NEAR(find(text, kI).y, -(250.0 + 200.0), kTol);
    // \limits and \nolimits.
    EXPECT_NEAR(find(typeset(R"(\sum\limits_i)"), kI).x, (1000.0 - 350.0) / 2.0, kTol);
    EXPECT_NEAR(find(typeset(R"(\sum\nolimits_i)", MathStyle::DISPLAY), kI).x, 1400.0, kTol);
}

TEST(MathLayout, IntegralsTuckTheirSubscriptIn)
{
    // The subscript goes left by the integral's italic correction (200).
    const MathBox integral = typeset(R"(\int_0^1)");
    EXPECT_NEAR(find(integral, U'1').x, 600.0, kTol);
    EXPECT_NEAR(find(integral, U'0').x, 400.0, kTol);
}

TEST(MathLayout, Accents)
{
    // Over the middle of an x (its ink's middle is the accent's place, -250 from its origin).
    const MathBox hat = typeset(R"(\hat{x})");
    EXPECT_NEAR(find(hat, 0x302).x, 500.0, kTol);
    EXPECT_NEAR(find(hat, 0x302).y, 0.0, kTol);
    EXPECT_NEAR(hat.width, 500.0, kTol);
    // Over a taller letter, raised by how much taller than the accent base height (500) it is;
    // at the letter's accent place.
    const MathBox capital = typeset(R"(\hat{A})");
    EXPECT_NEAR(find(capital, 0x302).y, 200.0, kTol);
    EXPECT_NEAR(find(capital, 0x302).x, 700.0, kTol);
    // An i loses its dot.
    const MathBox i = typeset(R"(\hat{i})");
    EXPECT_EQ(count(i, kI), 0U);
    EXPECT_EQ(count(i, kDotlessItalicI), 1U);
}

TEST(MathLayout, WideAccents)
{
    // The widest form not wider than the base.
    EXPECT_EQ(count(typeset(R"(\widehat{xyz})"), kHatWide), 1U);
    EXPECT_NEAR(find(typeset(R"(\widehat{xyz})"), kHatWide).x, 150.0, kTol);
    EXPECT_EQ(count(typeset(R"(\widehat{x})"), 0x302), 1U);
    // An arrow over its base, as wide as it: two extenders, 33 1/3 overlaps.
    const MathBox arrow = typeset(R"(\overrightarrow{xyz})");
    EXPECT_EQ(count(arrow, kArrowExtender), 2U);
    EXPECT_NEAR(find(arrow, kArrowLeftEnd).x, 0.0, kTol);
    EXPECT_NEAR(find(arrow, kArrowHead).x, 1000.0, kTol);
    EXPECT_NEAR(find(arrow, kArrowHead).y, 500.0 + 100.0 + 10.0, kTol);
}

TEST(MathLayout, OverAndUnderlines)
{
    const MathBox over = typeset(R"(\overline{x})");
    ASSERT_EQ(over.rules.size(), 1U);
    EXPECT_NEAR(over.rules[0].y, 600.0, kTol);
    EXPECT_NEAR(over.rules[0].width, 500.0, kTol);
    EXPECT_NEAR(over.ascent, 680.0, kTol);
    const MathBox under = typeset(R"(\underline{x})");
    ASSERT_EQ(under.rules.size(), 1U);
    EXPECT_NEAR(under.rules[0].y, -140.0, kTol);
    EXPECT_NEAR(under.depth, 180.0, kTol);
}

TEST(MathLayout, Delimiters)
{
    // Around an x the ( is its own size, centred on the axis already.
    const MathBox small = typeset(R"(\left(x\right))");
    EXPECT_NEAR(find(small, U'(').y, 0.0, kTol);
    EXPECT_NEAR(find(small, kX).x, 400.0, kTol);
    // \big to \Bigg: 0.85 to 1.75 em.
    EXPECT_EQ(count(typeset(R"(\big()"), U'('), 1U);
    EXPECT_EQ(count(typeset(R"(\Bigg()"), kParenVariant2), 1U);
    // An empty delimiter is a little space.
    EXPECT_NEAR(find(typeset(R"(\left.x\right))"), kX).x, 120.0, kTol);
}

TEST(MathLayout, MatricesAreCentredOnTheAxis)
{
    // Four rows, each a strut 840 high and 360 deep: 4800 in all, centred on the axis (250).
    const MathBox matrix = typeset(R"(\begin{matrix}a\\b\\a\\b\end{matrix})");
    EXPECT_NEAR(find(matrix, kA).y, 2650.0 - 840.0, kTol);
    EXPECT_NEAR(find(matrix, kB).y, 2650.0 - 840.0 - 1200.0, kTol);
    EXPECT_NEAR(matrix.ascent, 2650.0, kTol);
    EXPECT_NEAR((matrix.ascent - matrix.depth) / 2.0, 250.0, kTol);
    // Between delimiters as tall as \left and \right make them: an assembly of 12 extenders.
    const MathBox delimited = typeset(R"(\begin{pmatrix}a\\b\\a\\b\end{pmatrix})");
    EXPECT_EQ(count(delimited, kParenExtender), 12U);
}

TEST(MathLayout, CasesAlignTheirColumnsLeft)
{
    const MathBox cases = typeset(R"(\begin{cases}a&x\\bb&x\end{cases})");
    EXPECT_NEAR(find(cases, kA).x, find(cases, kB).x, kTol);
    // The second column a quad after the widest cell of the first.
    EXPECT_NEAR(find(cases, kX).x - find(cases, kA).x, 1000.0 + 1000.0, kTol);
}

TEST(MathLayout, TextIsUpright)
{
    const MathBox text = typeset(R"(\text{a b})");
    EXPECT_NEAR(find(text, U'a').x, 0.0, kTol);
    EXPECT_NEAR(find(text, U'b').x, 833.0, kTol);  // after the font's space
    EXPECT_EQ(count(text, kA), 0U);
}

TEST(MathLayout, AnEmptyFormulaIsAnEmptyBox)
{
    const MathBox empty = typeset("");
    EXPECT_TRUE(empty.glyphs.empty());
    EXPECT_NEAR(empty.width, 0.0, kTol);
}

}  // namespace
