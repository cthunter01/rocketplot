#include "core/Utf8.h"

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::appendUtf8;
using rocketplot::core::decodeUtf8;
using rocketplot::core::toUtf32;

TEST(Utf8, Decodes)
{
    EXPECT_EQ(toUtf32("x"), U"x");
    EXPECT_EQ(toUtf32("é"), U"é");
    EXPECT_EQ(toUtf32("≤"), U"≤");
    EXPECT_EQ(toUtf32("𝑥 = 1"), U"𝑥 = 1");
    EXPECT_EQ(toUtf32(""), U"");
}

TEST(Utf8, AnInvalidByteIsAReplacementCharacter)
{
    EXPECT_EQ(toUtf32("a\xff"
                      "b"),
              U"a�b");
    EXPECT_EQ(toUtf32("\xe2\x89"), U"��");            // cut short
    EXPECT_EQ(toUtf32("\xc0\xaf"), U"��");            // overlong
    EXPECT_EQ(toUtf32("\xed\xa0\x80"), U"���");       // a surrogate
    EXPECT_EQ(toUtf32("\xf4\x90\x80\x80"), U"����");  // past U+10FFFF
    std::size_t position = 0;
    EXPECT_EQ(decodeUtf8("\x80x", position), U'�');
    EXPECT_EQ(position, 1U);
}

TEST(Utf8, EncodesWhatItDecodes)
{
    for (const char32_t c : {U'x', U'é', U'≤', U'𝑥', U'\U0010FFFF'})
    {
        std::string text;
        appendUtf8(text, c);
        EXPECT_EQ(toUtf32(text), std::u32string(1, c));
    }
}

}  // namespace
