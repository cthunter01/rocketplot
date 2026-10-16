#include "core/MathPlainText.h"

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/MathParser.h"

namespace
{

using rocketplot::core::mathToPlainText;
using rocketplot::core::parseMath;

std::string plain(std::string_view tex)
{
    const auto list = parseMath(tex);
    return list ? mathToPlainText(*list) : "error";
}

TEST(MathPlainText, SymbolsAndScripts)
{
    EXPECT_EQ(plain("v_z"), "v_z");
    EXPECT_EQ(plain(R"(\alpha^2)"), "α^2");
    EXPECT_EQ(plain("x_{i+1}"), "x_(i+1)");
    EXPECT_EQ(plain("e^{-x}"), "e^(−x)");
    EXPECT_EQ(plain("f'(x)"), "f′(x)");
    EXPECT_EQ(plain("f''^2"), "f″^2");
    EXPECT_EQ(plain(R"(\sum_{i=1}^n x_i)"), "∑_(i=1)^n x_i");
}

TEST(MathPlainText, OperatorsAreSpaced)
{
    EXPECT_EQ(plain("F=ma"), "F = ma");
    EXPECT_EQ(plain("a-b"), "a − b");
    EXPECT_EQ(plain("-x"), "−x");
    EXPECT_EQ(plain("(-x)"), "(−x)");
    EXPECT_EQ(plain(R"(a\leq b)"), "a ≤ b");
    EXPECT_EQ(plain(R"(\sin x)"), "sin x");
    EXPECT_EQ(plain(R"(\sin(x))"), "sin(x)");
}

TEST(MathPlainText, Constructions)
{
    EXPECT_EQ(plain(R"(\frac{a+b}{c})"), "(a + b)/c");
    EXPECT_EQ(plain(R"(\frac12)"), "1/2");
    EXPECT_EQ(plain(R"(\binom{n}{k})"), "(n choose k)");
    EXPECT_EQ(plain(R"(\sqrt{x})"), "√x");
    EXPECT_EQ(plain(R"(\sqrt{x+1})"), "√(x + 1)");
    EXPECT_EQ(plain(R"(\sqrt[3]{x})"), "3√x");
    EXPECT_EQ(plain(R"(\hat{x})"), "x̂");
    EXPECT_EQ(plain(R"(\left(\frac{1}{2}\right))"), "(1/2)");
    EXPECT_EQ(plain(R"(\text{if } x>0)"), "if x > 0");
    EXPECT_EQ(plain(R"(a\quad b\,c)"), "a b c");
    EXPECT_EQ(plain(R"(\begin{pmatrix}1&0\\0&1\end{pmatrix})"), "(1, 0; 0, 1)");
}

TEST(MathPlainText, LettersThatMeanSomethingElseKeepTheirForm)
{
    EXPECT_EQ(plain(R"(x\in\mathbb{R})"), "x ∈ ℝ");
    EXPECT_EQ(plain(R"(\mathcal{L})"), "ℒ");
    EXPECT_EQ(plain(R"(\mathbf{F})"), "F");
    EXPECT_EQ(plain(R"(\mathrm{d}x)"), "dx");
}

}  // namespace
