#include "core/NumberFormatter.h"

#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::chooseTickFormat;
using rocketplot::core::decimalsForStep;
using rocketplot::core::formatTick;
using rocketplot::core::TickFormat;

constexpr std::string_view kMinus = "\u2212";

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

TEST(NumberFormatter, FixedNotation)
{
    const TickFormat format = chooseTickFormat(0.5, 10.0);
    EXPECT_EQ(format, (TickFormat{.scientific = false, .decimals = 1}));
    EXPECT_EQ(formatTick(2.5, format), "2.5");
    EXPECT_EQ(formatTick(3.0, format), "3.0");
    EXPECT_EQ(formatTick(-1.5, format), std::string(kMinus) + "1.5");
    EXPECT_EQ(formatTick(0.30000000000000004, chooseTickFormat(0.1, 1.0)), "0.3");
}

TEST(NumberFormatter, ZeroHasNoSign)
{
    EXPECT_EQ(formatTick(-0.0, chooseTickFormat(1.0, 10.0)), "0");
    EXPECT_EQ(formatTick(-1e-17, chooseTickFormat(0.1, 1.0)), "0");
    EXPECT_EQ(formatTick(0.0, chooseTickFormat(0.1, 1.0)), "0");
}

TEST(NumberFormatter, ScientificForLargeSteps)
{
    const TickFormat format = chooseTickFormat(2e6, 8e6);
    EXPECT_TRUE(format.scientific);
    EXPECT_EQ(formatTick(4e6, format), "4e6");
    EXPECT_EQ(formatTick(-6e6, format), std::string(kMinus) + "6e6");
}

TEST(NumberFormatter, ScientificForTinyMagnitudes)
{
    const TickFormat format = chooseTickFormat(2e-6, 1e-5);
    EXPECT_TRUE(format.scientific);
    EXPECT_EQ(formatTick(4e-6, format), "4.0e" + std::string(kMinus) + "6");
}

TEST(NumberFormatter, LargeValuesWithSmallStepsStayFixed)
{
    // Epoch seconds: scientific would hide the digits that differ.
    const TickFormat format = chooseTickFormat(0.5, 1.7e9);
    EXPECT_FALSE(format.scientific);
    EXPECT_EQ(formatTick(1700000000.5, format), "1700000000.5");
}

TEST(NumberFormatter, NonFiniteValues)
{
    const TickFormat format;
    EXPECT_EQ(formatTick(std::numeric_limits<double>::quiet_NaN(), format), "NaN");
    EXPECT_EQ(formatTick(std::numeric_limits<double>::infinity(), format), "∞");
    EXPECT_EQ(formatTick(-std::numeric_limits<double>::infinity(), format),
              std::string(kMinus) + "∞");
}

}  // namespace
