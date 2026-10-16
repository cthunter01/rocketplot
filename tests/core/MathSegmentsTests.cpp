#include "core/MathSegments.h"

#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::MathSegment;
using rocketplot::core::splitMath;

MathSegment text(const char* s)
{
    return {.text = s, .math = false};
}

MathSegment math(const char* s)
{
    return {.text = s, .math = true};
}

TEST(MathSegments, TextWithoutDollarsIsOneSegment)
{
    EXPECT_EQ(splitMath("Thrust (kN)"), std::vector{text("Thrust (kN)")});
    EXPECT_TRUE(splitMath("").empty());
}

TEST(MathSegments, PairsOfDollarsAlternateTextAndMath)
{
    EXPECT_EQ(splitMath("$v_z$"), std::vector{math("v_z")});
    EXPECT_EQ(splitMath("Speed $v$ (m/s)"),
              (std::vector{text("Speed "), math("v"), text(" (m/s)")}));
    EXPECT_EQ(splitMath("$a$ and $b$"), (std::vector{math("a"), text(" and "), math("b")}));
    EXPECT_EQ(splitMath("$a$$b$"), (std::vector{math("a"), math("b")}));
}

TEST(MathSegments, AnOddNumberOfDollarsIsAllText)
{
    EXPECT_EQ(splitMath("Cost ($)"), std::vector{text("Cost ($)")});
    EXPECT_EQ(splitMath("$5, $6 or $7"), std::vector{text("$5, $6 or $7")});
}

TEST(MathSegments, ABackslashEscapesADollar)
{
    // In text, the backslash goes.
    EXPECT_EQ(splitMath(R"(From \$5 to \$10)"), std::vector{text("From $5 to $10")});
    EXPECT_EQ(splitMath(R"(\$$x$)"), (std::vector{text("$"), math("x")}));
    // In a formula, it stays for the TeX parser.
    EXPECT_EQ(splitMath(R"($\$5 + x$)"), std::vector{math(R"(\$5 + x)")});
    // An escaped dollar doesn't count: one unescaped dollar is all text.
    EXPECT_EQ(splitMath(R"($5 or \$6)"), std::vector{text("$5 or $6")});
}

TEST(MathSegments, EmptyFormulasAreLeftOut)
{
    EXPECT_EQ(splitMath("a$$b"), (std::vector{text("a"), text("b")}));
    EXPECT_TRUE(splitMath("$$").empty());
}

TEST(MathSegments, Utf8PassesThrough)
{
    EXPECT_EQ(splitMath("Δv = $\\Delta v$ (m·s⁻¹)"),
              (std::vector{text("Δv = "), math("\\Delta v"), text(" (m·s⁻¹)")}));
}

}  // namespace
