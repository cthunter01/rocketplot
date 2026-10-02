#include "core/NumberFormatter.h"

#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::chooseLabeling;
using rocketplot::core::decimalsForStep;
using rocketplot::core::formatLabel;
using rocketplot::core::formatLogLabel;
using rocketplot::core::formatValue;
using rocketplot::core::Labeling;
using rocketplot::core::labelingAnnotation;
using rocketplot::core::NumberStyle;
using rocketplot::core::superscript;

constexpr std::string_view kMinusSign = "\u2212";
constexpr std::string_view kTimesSign = "\u00D7";

TEST(NumberFormatter, DecimalsForStep)
{
    EXPECT_EQ(decimalsForStep(5.0), 0);
    EXPECT_EQ(decimalsForStep(100.0), 0);
    EXPECT_EQ(decimalsForStep(0.5), 1);
    EXPECT_EQ(decimalsForStep(0.1), 1);
    EXPECT_EQ(decimalsForStep(0.25), 2);
    EXPECT_EQ(decimalsForStep(0.002), 3);
    EXPECT_EQ(decimalsForStep(0.0), 0);
}

TEST(NumberFormatter, PlainLabels)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = -3.0, .max = 7.0}, 0.5, NumberStyle::AUTO);
    EXPECT_EQ(labelingAnnotation(labeling), "");
    EXPECT_EQ(formatLabel(2.5, labeling), "2.5");
    EXPECT_EQ(formatLabel(3.0, labeling), "3.0");
    EXPECT_EQ(formatLabel(-1.5, labeling), std::string(kMinusSign) + "1.5");
    EXPECT_EQ(formatLabel(0.30000000000000004,
                          chooseLabeling({.min = 0, .max = 1}, 0.1, NumberStyle::AUTO)),
              "0.3");
}

TEST(NumberFormatter, ZeroHasNoSign)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = -1.0, .max = 1.0}, 0.1, NumberStyle::AUTO);
    EXPECT_EQ(formatLabel(-0.0, labeling), "0");
    EXPECT_EQ(formatLabel(-1e-17, labeling), "0");
}

TEST(NumberFormatter, OffsetForLargeValuesWithASmallRange)
{
    // Epoch seconds: 1700000000.1 .. .9 is labeled 0.1 .. 0.9, "+1.7×10⁹".
    const Labeling labeling =
        chooseLabeling(Range{.min = 1.7e9 + 0.1, .max = 1.7e9 + 0.9}, 0.1, NumberStyle::AUTO);
    EXPECT_DOUBLE_EQ(labeling.offset, 1.7e9);
    EXPECT_EQ(labeling.exponent, 0);
    EXPECT_EQ(formatLabel(1.7e9 + 0.5, labeling), "0.5");
    EXPECT_EQ(labelingAnnotation(labeling), "+1.7" + std::string(kTimesSign) + "10⁹");
}

TEST(NumberFormatter, OffsetAcrossAPowerOfTen)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = 1699999999.5, .max = 1700000000.3}, 0.1, NumberStyle::AUTO);
    EXPECT_DOUBLE_EQ(labeling.offset, 1.7e9);
    EXPECT_EQ(formatLabel(1699999999.6, labeling), std::string(kMinusSign) + "0.4");
}

TEST(NumberFormatter, NegativeOffset)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = -500020.0, .max = -500010.0}, 2.0, NumberStyle::AUTO);
    EXPECT_DOUBLE_EQ(labeling.offset, -500000.0);
    EXPECT_EQ(formatLabel(-500016.0, labeling), std::string(kMinusSign) + "16");
    EXPECT_EQ(labelingAnnotation(labeling), std::string(kMinusSign) + "500000");
}

TEST(NumberFormatter, NoOffsetAroundZeroOrForShortNumbers)
{
    EXPECT_EQ(chooseLabeling(Range{.min = -5.0, .max = 5.0}, 1.0, NumberStyle::AUTO).offset, 0.0);
    EXPECT_EQ(chooseLabeling(Range{.min = 100.0, .max = 200.0}, 20.0, NumberStyle::AUTO).offset,
              0.0);
}

TEST(NumberFormatter, MultiplierForHugeAndTinyValues)
{
    const Labeling huge = chooseLabeling(Range{.min = 0.0, .max = 8e6}, 2e6, NumberStyle::AUTO);
    EXPECT_EQ(huge.exponent, 6);
    EXPECT_EQ(formatLabel(4e6, huge), "4");
    EXPECT_EQ(labelingAnnotation(huge), std::string(kTimesSign) + "10⁶");

    const Labeling tiny = chooseLabeling(Range{.min = 0.0, .max = 8e-6}, 2e-6, NumberStyle::AUTO);
    EXPECT_EQ(tiny.exponent, -6);
    EXPECT_EQ(formatLabel(6e-6, tiny), "6");
    EXPECT_EQ(labelingAnnotation(tiny), std::string(kTimesSign) + "10⁻⁶");

    // 40000 stays as it is: 10^4 is still readable.
    EXPECT_EQ(
        chooseLabeling(Range{.min = 0.0, .max = 40000.0}, 10000.0, NumberStyle::AUTO).exponent, 0);
}

TEST(NumberFormatter, SiPrefixesPerLabel)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = 0.0, .max = 1500.0}, 250.0, NumberStyle::SI);
    EXPECT_EQ(formatLabel(0.0, labeling), "0");
    EXPECT_EQ(formatLabel(750.0, labeling), "750");
    EXPECT_EQ(formatLabel(1000.0, labeling), "1.00k");
    EXPECT_EQ(formatLabel(1250.0, labeling), "1.25k");
    EXPECT_EQ(labelingAnnotation(labeling), "");

    const Labeling small = chooseLabeling(Range{.min = 0.0, .max = 3e-6}, 5e-7, NumberStyle::SI);
    EXPECT_EQ(formatLabel(2.5e-6, small), "2.5µ");
    EXPECT_EQ(formatLabel(-5e-7, small), std::string(kMinusSign) + "500n");
}

TEST(NumberFormatter, SiOffset)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = 1.5e9 + 1.0, .max = 1.5e9 + 9.0}, 2.0, NumberStyle::SI);
    EXPECT_EQ(formatLabel(1.5e9 + 4.0, labeling), "4");
    EXPECT_EQ(labelingAnnotation(labeling), "+1.5G");
}

TEST(NumberFormatter, PlainStyleNeverAbbreviates)
{
    const Labeling labeling =
        chooseLabeling(Range{.min = 1.7e9 + 0.1, .max = 1.7e9 + 0.9}, 0.1, NumberStyle::PLAIN);
    EXPECT_EQ(labelingAnnotation(labeling), "");
    EXPECT_EQ(formatLabel(1.7e9 + 0.5, labeling), "1700000000.5");
}

TEST(NumberFormatter, LogLabels)
{
    EXPECT_EQ(formatLogLabel(0.001, NumberStyle::AUTO), "0.001");
    EXPECT_EQ(formatLogLabel(100.0, NumberStyle::AUTO), "100");
    EXPECT_EQ(formatLogLabel(10000.0, NumberStyle::AUTO), "10000");
    EXPECT_EQ(formatLogLabel(1e5, NumberStyle::AUTO), "10⁵");
    EXPECT_EQ(formatLogLabel(1e-6, NumberStyle::AUTO), "10⁻⁶");
    EXPECT_EQ(formatLogLabel(2e8, NumberStyle::AUTO), "2" + std::string(kTimesSign) + "10⁸");
    EXPECT_EQ(formatLogLabel(1e5, NumberStyle::SI), "100k");
    EXPECT_EQ(formatLogLabel(0.002, NumberStyle::SI), "2m");
    EXPECT_EQ(formatLogLabel(0.0, NumberStyle::AUTO), "");
}

TEST(NumberFormatter, Superscript)
{
    EXPECT_EQ(superscript(0), "⁰");
    EXPECT_EQ(superscript(12), "¹²");
    EXPECT_EQ(superscript(-3), "⁻³");
}

TEST(NumberFormatter, NonFiniteValues)
{
    const Labeling labeling;
    EXPECT_EQ(formatLabel(std::numeric_limits<double>::quiet_NaN(), labeling), "NaN");
    EXPECT_EQ(formatLabel(std::numeric_limits<double>::infinity(), labeling), "∞");
    EXPECT_EQ(formatLabel(-std::numeric_limits<double>::infinity(), labeling),
              std::string(kMinusSign) + "∞");
}

TEST(NumberFormatter, ValuesToAResolution)
{
    const std::string minus(kMinusSign);
    const std::string times(kTimesSign);
    EXPECT_EQ(formatValue(3.14159, 0.01, NumberStyle::AUTO), "3.14");
    EXPECT_EQ(formatValue(3.14159, 0.03, NumberStyle::AUTO), "3.14");
    EXPECT_EQ(formatValue(-2.5, 0.5, NumberStyle::AUTO), minus + "2.5");
    EXPECT_EQ(formatValue(1234.567, 20.0, NumberStyle::AUTO), "1235");
    EXPECT_EQ(formatValue(-0.0004, 0.01, NumberStyle::AUTO), "0");
    // Large and tiny numbers: ×10ⁿ, unless they need many digits.
    EXPECT_EQ(formatValue(3.2e9, 1e7, NumberStyle::AUTO), "3.20" + times + "10⁹");
    EXPECT_EQ(formatValue(-1.5e-7, 1e-9, NumberStyle::AUTO), minus + "1.50" + times + "10⁻⁷");
    EXPECT_EQ(formatValue(9.9996e9, 1e6, NumberStyle::AUTO), "1.000" + times + "10¹⁰");
    EXPECT_EQ(formatValue(1700000000.1234, 0.001, NumberStyle::AUTO), "1700000000.123");
    // SI and plain.
    EXPECT_EQ(formatValue(1234.5, 0.1, NumberStyle::SI), "1.2345k");
    EXPECT_EQ(formatValue(0.5, 0.001, NumberStyle::SI), "500m");
    EXPECT_EQ(formatValue(3.2e9, 1e7, NumberStyle::PLAIN), "3200000000");
    // No usable resolution: as many digits as the value has.
    EXPECT_EQ(formatValue(0.1, 0.0, NumberStyle::AUTO), "0.1");
    EXPECT_EQ(formatValue(std::numeric_limits<double>::quiet_NaN(), 1.0, NumberStyle::AUTO), "NaN");
}

}  // namespace
