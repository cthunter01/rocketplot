#include "core/MathSymbols.h"

#include <optional>
#include <set>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::AtomType;
using rocketplot::core::findAccent;
using rocketplot::core::findAlphabet;
using rocketplot::core::findDelimiter;
using rocketplot::core::findOperatorName;
using rocketplot::core::findSpace;
using rocketplot::core::findSymbol;
using rocketplot::core::Limits;
using rocketplot::core::MathAccent;
using rocketplot::core::MathAlphabet;
using rocketplot::core::mathCharacter;
using rocketplot::core::MathSymbol;
using rocketplot::core::negated;
using rocketplot::core::OperatorName;
using rocketplot::core::styledCharacter;
using rocketplot::core::typeOf;

MathSymbol ord(char32_t c)
{
    return {.codePoint = c, .type = AtomType::ORD};
}

TEST(MathSymbols, CommandsStandForCharacters)
{
    EXPECT_EQ(findSymbol("alpha"), ord(U'α'));
    EXPECT_EQ(findSymbol("Gamma"), ord(U'Γ'));
    EXPECT_EQ(findSymbol("epsilon"), ord(U'ϵ'));  // TeX's is the lunate one
    EXPECT_EQ(findSymbol("varepsilon"), ord(U'ε'));
    EXPECT_EQ(findSymbol("leq"), (MathSymbol{.codePoint = U'≤', .type = AtomType::REL}));
    EXPECT_EQ(findSymbol("le"), findSymbol("leq"));
    EXPECT_EQ(findSymbol("times"), (MathSymbol{.codePoint = U'×', .type = AtomType::BIN}));
    EXPECT_EQ(findSymbol("langle"), (MathSymbol{.codePoint = U'⟨', .type = AtomType::OPEN}));
    EXPECT_EQ(findSymbol("}"), (MathSymbol{.codePoint = U'}', .type = AtomType::CLOSE}));
    EXPECT_EQ(findSymbol("foo"), std::nullopt);
    EXPECT_EQ(findSymbol(""), std::nullopt);
}

TEST(MathSymbols, LargeOperators)
{
    EXPECT_EQ(findSymbol("sum"),
              (MathSymbol{.codePoint = U'∑', .type = AtomType::OP, .limits = Limits::DISPLAY}));
    EXPECT_EQ(findSymbol("int"),
              (MathSymbol{.codePoint = U'∫', .type = AtomType::OP, .limits = Limits::NO_LIMITS}));
    EXPECT_EQ(findOperatorName("lim"), (OperatorName{.text = "lim", .limits = Limits::DISPLAY}));
    EXPECT_EQ(findOperatorName("sin"), (OperatorName{.text = "sin", .limits = Limits::NO_LIMITS}));
    EXPECT_EQ(findOperatorName("limsup"),
              (OperatorName{.text = "lim sup", .limits = Limits::DISPLAY}));
    EXPECT_EQ(findOperatorName("alpha"), std::nullopt);
}

TEST(MathSymbols, Delimiters)
{
    EXPECT_EQ(findDelimiter(U'('), U'(');
    EXPECT_EQ(findDelimiter(U'.'), 0);  // none
    EXPECT_EQ(findDelimiter(U'<'), U'⟨');
    EXPECT_EQ(findDelimiter(U'x'), std::nullopt);
    EXPECT_EQ(findDelimiter("langle"), U'⟨');
    EXPECT_EQ(findDelimiter("|"), U'‖');
    EXPECT_EQ(findDelimiter("{"), U'{');
    EXPECT_EQ(findDelimiter("alpha"), std::nullopt);
}

TEST(MathSymbols, AccentsAlphabetsAndSpaces)
{
    EXPECT_EQ(findAccent("hat"), (MathAccent{.codePoint = 0x302, .wide = false}));
    EXPECT_EQ(findAccent("widehat"), (MathAccent{.codePoint = 0x302, .wide = true}));
    EXPECT_EQ(findAccent("vec"), (MathAccent{.codePoint = 0x20D7, .wide = false}));
    EXPECT_EQ(findAlphabet("mathbb"), MathAlphabet::DOUBLE_STRUCK);
    EXPECT_EQ(findAlphabet("mathcal"), MathAlphabet::SCRIPT);
    EXPECT_EQ(findSpace(","), 3.0);
    EXPECT_EQ(findSpace("quad"), 18.0);
    EXPECT_EQ(findSpace("!"), -3.0);
}

TEST(MathSymbols, Negations)
{
    EXPECT_EQ(negated(U'='), U'≠');
    EXPECT_EQ(negated(U'∈'), U'∉');
    EXPECT_EQ(negated(U'≤'), U'≰');
    EXPECT_EQ(negated(U'x'), std::nullopt);
}

TEST(MathSymbols, TypedCharacters)
{
    EXPECT_EQ(typeOf(U'+'), AtomType::BIN);
    EXPECT_EQ(typeOf(U'-'), AtomType::BIN);
    EXPECT_EQ(typeOf(U'='), AtomType::REL);
    EXPECT_EQ(typeOf(U'('), AtomType::OPEN);
    EXPECT_EQ(typeOf(U']'), AtomType::CLOSE);
    EXPECT_EQ(typeOf(U','), AtomType::PUNCT);
    EXPECT_EQ(typeOf(U'x'), AtomType::ORD);
    EXPECT_EQ(typeOf(U'≤'), AtomType::REL);
    EXPECT_EQ(typeOf(U'∑'), AtomType::OP);
    EXPECT_EQ(typeOf(U'α'), AtomType::ORD);
    EXPECT_EQ(mathCharacter(U'-'), U'−');
    EXPECT_EQ(mathCharacter(U'*'), U'∗');
    EXPECT_EQ(mathCharacter(U'x'), U'x');
}

TEST(MathSymbols, TeXsDefaultAlphabet)
{
    EXPECT_EQ(styledCharacter(U'x', MathAlphabet::DEFAULT), U'𝑥');
    EXPECT_EQ(styledCharacter(U'A', MathAlphabet::DEFAULT), U'𝐴');
    EXPECT_EQ(styledCharacter(U'h', MathAlphabet::DEFAULT), U'ℎ');  // Planck's constant
    EXPECT_EQ(styledCharacter(U'1', MathAlphabet::DEFAULT), U'1');
    EXPECT_EQ(styledCharacter(U'α', MathAlphabet::DEFAULT), U'𝛼');
    EXPECT_EQ(styledCharacter(U'ϵ', MathAlphabet::DEFAULT), U'𝜖');
    EXPECT_EQ(styledCharacter(U'Γ', MathAlphabet::DEFAULT), U'Γ');  // capitals upright
    EXPECT_EQ(styledCharacter(U'∂', MathAlphabet::DEFAULT), U'∂');
    EXPECT_EQ(styledCharacter(U'ı', MathAlphabet::DEFAULT), U'𝚤');
    EXPECT_EQ(styledCharacter(U'+', MathAlphabet::DEFAULT), U'+');
}

TEST(MathSymbols, OtherAlphabets)
{
    EXPECT_EQ(styledCharacter(U'x', MathAlphabet::UPRIGHT), U'x');
    EXPECT_EQ(styledCharacter(U'Γ', MathAlphabet::ITALIC), U'𝛤');
    EXPECT_EQ(styledCharacter(U'x', MathAlphabet::BOLD), U'𝐱');
    EXPECT_EQ(styledCharacter(U'1', MathAlphabet::BOLD), U'𝟏');
    EXPECT_EQ(styledCharacter(U'Γ', MathAlphabet::BOLD), U'𝚪');
    EXPECT_EQ(styledCharacter(U'∇', MathAlphabet::BOLD), U'𝛁');
    EXPECT_EQ(styledCharacter(U'α', MathAlphabet::BOLD_ITALIC), U'𝜶');
    EXPECT_EQ(styledCharacter(U'Γ', MathAlphabet::BOLD_ITALIC), U'𝚪');
    EXPECT_EQ(styledCharacter(U'R', MathAlphabet::DOUBLE_STRUCK), U'ℝ');
    EXPECT_EQ(styledCharacter(U'A', MathAlphabet::DOUBLE_STRUCK), U'𝔸');
    EXPECT_EQ(styledCharacter(U'1', MathAlphabet::DOUBLE_STRUCK), U'𝟙');
    EXPECT_EQ(styledCharacter(U'L', MathAlphabet::SCRIPT), U'ℒ');
    EXPECT_EQ(styledCharacter(U'A', MathAlphabet::SCRIPT), U'𝒜');
    EXPECT_EQ(styledCharacter(U'g', MathAlphabet::FRAKTUR), U'𝔤');
    EXPECT_EQ(styledCharacter(U'R', MathAlphabet::FRAKTUR), U'ℜ');
    EXPECT_EQ(styledCharacter(U'x', MathAlphabet::SANS_SERIF), U'𝗑');
    EXPECT_EQ(styledCharacter(U'x', MathAlphabet::MONOSPACE), U'𝚡');
    EXPECT_EQ(styledCharacter(U'α', MathAlphabet::DOUBLE_STRUCK), U'α');  // none: as it is
}

// The forms of the 52 Latin letters in @p alphabet.
std::set<char32_t> latinForms(MathAlphabet alphabet)
{
    std::set<char32_t> forms;
    for (const char32_t first : {U'A', U'a'})
    {
        for (char32_t letter = first; letter < first + 26; ++letter)
        {
            forms.insert(styledCharacter(letter, alphabet));
        }
    }
    return forms;
}

TEST(MathSymbols, NoLetterLandsInAHoleOfItsAlphabet)
{
    // The places in Mathematical Alphanumeric Symbols that Unicode leaves empty, because the
    // letter was in Letterlike Symbols already.
    const std::set<char32_t> holes = {
        0x1D455, 0x1D49D, 0x1D4A0, 0x1D4A1, 0x1D4A3, 0x1D4A4, 0x1D4A7, 0x1D4A8,
        0x1D4AD, 0x1D4BA, 0x1D4BC, 0x1D4C4, 0x1D506, 0x1D50B, 0x1D50C, 0x1D515,
        0x1D51D, 0x1D53A, 0x1D53F, 0x1D545, 0x1D547, 0x1D548, 0x1D549, 0x1D551,
    };
    const std::vector<MathAlphabet> alphabets = {
        MathAlphabet::DEFAULT,     MathAlphabet::ITALIC,        MathAlphabet::BOLD,
        MathAlphabet::BOLD_ITALIC, MathAlphabet::DOUBLE_STRUCK, MathAlphabet::SCRIPT,
        MathAlphabet::FRAKTUR,     MathAlphabet::SANS_SERIF,    MathAlphabet::MONOSPACE,
    };
    for (const MathAlphabet alphabet : alphabets)
    {
        const std::set<char32_t> forms = latinForms(alphabet);
        EXPECT_EQ(forms.size(), 52U) << static_cast<int>(alphabet);  // all different
        for (const char32_t form : forms)
        {
            EXPECT_FALSE(holes.contains(form)) << static_cast<int>(alphabet);
            EXPECT_GE(form, 0x2100U) << static_cast<int>(alphabet);  // none left as it was
        }
    }
}

}  // namespace
