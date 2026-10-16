#include "core/MathParser.h"

#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "MathDump.h"
#include "core/MathAtom.h"
#include "core/MathSymbols.h"

namespace
{

using rocketplot::core::AtomType;
using rocketplot::core::Limits;
using rocketplot::core::MathList;
using rocketplot::core::parseMath;
using rocketplot::test::dump;

// The formula's math list as dump() has it, or its error: "error at 3: Double superscript".
std::string parsed(std::string_view tex)
{
    const auto list = parseMath(tex);
    if (!list)
    {
        return "error at " + std::to_string(list.error().position) + ": " + list.error().message;
    }
    return dump(*list);
}

TEST(MathParser, CharactersAndSymbols)
{
    EXPECT_EQ(parsed("x + 1"), "x + 1");  // spaces don't count in math
    EXPECT_EQ(parsed("a-b*c"), "a − b ∗ c");
    EXPECT_EQ(parsed(R"(\alpha\leq\infty)"), "α ≤ ∞");
    EXPECT_EQ(parsed("Δv ≤ 3"), "Δ v ≤ 3");
    EXPECT_EQ(parsed(R"(\{\}\%\$\_\|)"), "{ } % $ _ ‖");
    EXPECT_EQ(parsed(""), "");
}

TEST(MathParser, AtomsHaveTeXsClasses)
{
    const auto list = parseMath(R"(a+b=\sum(c),\leq)");
    ASSERT_TRUE(list);
    std::vector<AtomType> types;
    for (const auto& atom : *list)
    {
        types.push_back(atom.type);
    }
    EXPECT_EQ(types, (std::vector{AtomType::ORD, AtomType::BIN, AtomType::ORD, AtomType::REL,
                                  AtomType::OP, AtomType::OPEN, AtomType::ORD, AtomType::CLOSE,
                                  AtomType::PUNCT, AtomType::REL}));
}

TEST(MathParser, Scripts)
{
    EXPECT_EQ(parsed("x^2"), "x^(2)");
    EXPECT_EQ(parsed("x_i^2"), "x_(i)^(2)");
    EXPECT_EQ(parsed("x^2_i"), "x_(i)^(2)");
    EXPECT_EQ(parsed("x^{ab}"), "x^(a b)");
    EXPECT_EQ(parsed(R"(x^\alpha)"), "x^(α)");
    EXPECT_EQ(parsed(R"(e^\frac12)"), "e^(frac(1,2))");
    EXPECT_EQ(parsed("{a+b}^2"), "{a + b}^(2)");
    EXPECT_EQ(parsed("^2"), "{}^(2)");  // on nothing
    EXPECT_EQ(parsed("x^2^3"), "error at 3: Double superscript");
    EXPECT_EQ(parsed("x_1_2"), "error at 3: Double subscript");
    EXPECT_EQ(parsed("x^"), "error at 2: Missing argument");
    EXPECT_EQ(parsed("x^}"), "error at 2: Missing argument");
}

TEST(MathParser, Primes)
{
    EXPECT_EQ(parsed("f'"), "f^(′)");
    EXPECT_EQ(parsed("f''(x)"), "f^(″) ( x )");
    EXPECT_EQ(parsed("f'^2"), "f^(′ 2)");
    EXPECT_EQ(parsed("f'_1"), "f_(1)^(′)");
    EXPECT_EQ(parsed("f'''''"), "f^(⁗ ′)");
    EXPECT_EQ(parsed("f^2'"), "error at 3: Double superscript");
    EXPECT_EQ(parsed("f'^2^3"), "error at 4: Double superscript");
}

TEST(MathParser, Fractions)
{
    EXPECT_EQ(parsed(R"(\frac{a+b}{c})"), "frac(a + b,c)");
    EXPECT_EQ(parsed(R"(\frac12)"), "frac(1,2)");
    EXPECT_EQ(parsed(R"(\frac 1 {x})"), "frac(1,x)");
    EXPECT_EQ(parsed(R"(\dfrac{1}{2})"), "frac[D](1,2)");
    EXPECT_EQ(parsed(R"(\tfrac{1}{2})"), "frac[T](1,2)");
    EXPECT_EQ(parsed(R"(\binom{n}{k})"), "binom(n,k)");
    EXPECT_EQ(parsed(R"(\frac{1})"), "error at 8: Missing argument");
}

TEST(MathParser, Radicals)
{
    EXPECT_EQ(parsed(R"(\sqrt{x})"), "sqrt(x)");
    EXPECT_EQ(parsed(R"(\sqrt2)"), "sqrt(2)");
    EXPECT_EQ(parsed(R"(\sqrt[3]{x})"), "sqrt[3](x)");
    EXPECT_EQ(parsed(R"(\sqrt[n+1]x)"), "sqrt[n + 1](x)");
    EXPECT_EQ(parsed(R"(\sqrt[{]}]x)"), "sqrt[{]}](x)");  // a ] in braces doesn't end the degree
    EXPECT_EQ(parsed(R"(\sqrt[3)"), "error at 0: Missing ]");
}

TEST(MathParser, Delimiters)
{
    EXPECT_EQ(parsed(R"(\left(\frac12\right))"), "left( frac(1,2) right)");
    EXPECT_EQ(parsed(R"(\left.x\right|)"), "left. x right|");
    EXPECT_EQ(parsed(R"(\left\langle x\right\rangle)"), "left⟨ x right⟩");
    EXPECT_EQ(parsed(R"(\left<x\right>)"), "left⟨ x right⟩");
    EXPECT_EQ(parsed(R"(\left(x)"), "error at 0: \\left without \\right");
    EXPECT_EQ(parsed(R"(x\right))"), "error at 1: \\right without \\left");
    EXPECT_EQ(parsed(R"(\left x\right))"), "error at 6: Missing delimiter");
    EXPECT_EQ(parsed(R"(\bigl(x\Bigr]\bigg|\Biggm\|)"), "big1l( x big2r] big3| big4m‖");
}

TEST(MathParser, Accents)
{
    EXPECT_EQ(parsed(R"(\hat{x})"), "accent[302](x)");
    EXPECT_EQ(parsed(R"(\vec v)"), "accent[20d7](v)");
    EXPECT_EQ(parsed(R"(\widehat{xyz})"), "accent[302w](x y z)");
    EXPECT_EQ(parsed(R"(\overline{AB}\underline x)"), "overline(A B) underline(x)");
}

TEST(MathParser, Alphabets)
{
    EXPECT_EQ(parsed(R"(\mathbf{x}y)"), "{x.bf} y");
    EXPECT_EQ(parsed(R"(\mathbb R)"), "{R.bb}");
    EXPECT_EQ(parsed(R"(\mathrm{d}x)"), "{d.rm} x");
    EXPECT_EQ(parsed(R"(\mathcal{L}\mathfrak g\mathsf A\mathtt B\mathit\Gamma\boldsymbol\alpha)"),
              "{L.cal} {g.frak} {A.sf} {B.tt} {Γ.it} {α.bi}");
    EXPECT_EQ(parsed(R"(\mathbf{\mathrm{a}b})"), "{{a.rm} b.bf}");  // the inner one wins
}

TEST(MathParser, TextAndOperatorNames)
{
    EXPECT_EQ(parsed(R"(\text{if } x)"), "text(if ).rm x");
    EXPECT_EQ(parsed(R"(\text{a {b} \$5})"), "text(a {b} $5).rm");
    EXPECT_EQ(parsed(R"(\textbf{F})"), "text(F).bf");
    EXPECT_EQ(parsed(R"(\sin x)"), "op(sin) x");
    EXPECT_EQ(parsed(R"(\lim_{x\to0})"), "op[limits](lim)_(x → 0)");
    EXPECT_EQ(parsed(R"(\liminf)"), "op[limits](lim inf)");
    EXPECT_EQ(parsed(R"(\operatorname{tr}A)"), "op(tr).rm A");
    EXPECT_EQ(parsed(R"(\operatorname*{argmax})"), "op[limits](argmax).rm");
    EXPECT_EQ(parsed(R"(\text x)"), "error at 6: Missing {");
    EXPECT_EQ(parsed(R"(\text{x)"), "error at 5: Missing }");
}

TEST(MathParser, Limits)
{
    const auto limitsOf = [](std::string_view tex) {
        const auto list = parseMath(tex);
        return list && !list->empty() ? list->front().limits : Limits::NO_LIMITS;
    };
    EXPECT_EQ(limitsOf(R"(\sum_{i=1}^n)"), Limits::DISPLAY);
    EXPECT_EQ(limitsOf(R"(\int_0^1)"), Limits::NO_LIMITS);
    EXPECT_EQ(limitsOf(R"(\int\limits_0^1)"), Limits::LIMITS);
    EXPECT_EQ(limitsOf(R"(\sum\nolimits_i)"), Limits::NO_LIMITS);
    EXPECT_EQ(parsed(R"(x\limits)"), "error at 1: \\limits must follow an operator");
}

TEST(MathParser, SpacesAndStyles)
{
    EXPECT_EQ(parsed(R"(a\,b\quad c~d\ e\!f)"),
              "a space(3) b space(18) c space(6) d space(6) e space(-3) f");
    EXPECT_EQ(parsed(R"(\displaystyle x\scriptstyle y)"), "style(D) x style(S) y");
}

TEST(MathParser, Environments)
{
    EXPECT_EQ(parsed(R"(\begin{pmatrix}a&b\\c&d\end{pmatrix})"), "array[(,)](a,b;c,d)");
    EXPECT_EQ(parsed(R"(\begin{matrix}1&0\\0&1\\\end{matrix})"), "array[.,.](1,0;0,1)");
    EXPECT_EQ(parsed(R"(\begin{bmatrix}x\end{bmatrix})"), "array[[,]](x)");
    EXPECT_EQ(parsed(R"(\begin{cases}1&x>0\\0&\text{otherwise}\end{cases})"),
              "array[{,.,left](1,x > 0;0,text(otherwise).rm)");
    EXPECT_EQ(parsed(R"(\begin{tabular}x\end{tabular})"),
              "error at 0: Unknown environment tabular");
    EXPECT_EQ(parsed(R"(\begin{matrix}x\end{pmatrix})"),
              "error at 15: \\begin{matrix} ended by another \\end");
    EXPECT_EQ(parsed(R"(\begin{matrix}x)"), "error at 0: \\begin{matrix} without \\end");
    EXPECT_EQ(parsed("a&b"), "error at 1: & outside a matrix or cases environment");
    EXPECT_EQ(parsed(R"(a\\b)"), "error at 1: \\\\ outside a matrix or cases environment");
    EXPECT_EQ(parsed(R"(\end{matrix})"), "error at 0: \\end without \\begin");
}

TEST(MathParser, Negations)
{
    EXPECT_EQ(parsed(R"(a\not=b)"), "a ≠ b");
    EXPECT_EQ(parsed(R"(x\not\in A)"), "x ∉ A");
    EXPECT_EQ(parsed(R"(\not x)"), "error at 0: \\not of something without a negation");
}

TEST(MathParser, Errors)
{
    EXPECT_EQ(parsed(R"(a+\foo)"), "error at 2: Unknown command \\foo");
    EXPECT_EQ(parsed("{x"), "error at 0: Missing }");
    EXPECT_EQ(parsed("x}"), "error at 1: Unexpected }");
    EXPECT_EQ(parsed("\\"), "error at 0: Unknown command \\");
}

TEST(MathParser, NestingIsLimited)
{
    const auto nested = [](int depth) {
        return std::string(static_cast<std::size_t>(depth), '{') + "x" +
               std::string(static_cast<std::size_t>(depth), '}');
    };
    EXPECT_TRUE(parseMath(nested(30)));
    EXPECT_EQ(parsed(nested(40)), "error at 32: Nested too deeply");
    EXPECT_FALSE(parseMath(std::string(10000, '{')));
    std::string fractions;
    for (int i = 0; i < 100; ++i)
    {
        fractions += R"(\frac1)";
    }
    EXPECT_FALSE(parseMath(fractions + "2"));
}

TEST(MathParser, AnythingParsesOrFails)
{
    // Random strings of TeX's tokens: each is a formula or an error, never a crash.
    const std::vector<std::string_view> tokens = {
        "x",
        "1",
        "+",
        "{",
        "}",
        "^",
        "_",
        "'",
        "&",
        "~",
        "\\\\",
        "[",
        "]",
        "(",
        ")",
        ".",
        "|",
        "<",
        "\\frac",
        "\\sqrt",
        "\\left",
        "\\right",
        "\\big",
        "\\hat",
        "\\mathbf",
        "\\text",
        "\\sum",
        "\\limits",
        "\\begin{pmatrix}",
        "\\end{pmatrix}",
        "\\begin{cases}",
        "\\end{cases}",
        "\\not",
        "\\operatorname*",
        "\\alpha",
        "\\,",
        "\\displaystyle",
        "\\",
        " ",
        "é",
        "\xff",
    };
    // NOLINTNEXTLINE(bugprone-random-generator-seed): reproducible test data
    std::mt19937                               generator(20261016);
    std::uniform_int_distribution<std::size_t> pick(0, tokens.size() - 1);
    std::uniform_int_distribution<int>         length(1, 24);
    int                                        formulas = 0;
    for (int i = 0; i < 20000; ++i)
    {
        std::string tex;
        for (int n = length(generator); n > 0; --n)
        {
            tex += tokens[pick(generator)];
        }
        formulas += parseMath(tex) ? 1 : 0;
    }
    EXPECT_GT(formulas, 0);
}

}  // namespace
