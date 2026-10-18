#include "core/AxisTicks.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/NumberFormatter.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::AxisTicks;
using rocketplot::core::formatReadout;
using rocketplot::core::makeTicks;
using rocketplot::core::NumberStyle;
using rocketplot::core::TickKind;

TEST(AxisTicks, LinearLabelsAndAnnotation)
{
    const auto ticks = makeTicks({
        .range        = Range{.min = 0.0, .max = 8e6},
        .lengthPx     = 400.0,
        .minSpacingPx = 80.0,
    });
    ASSERT_EQ(ticks.labels.size(), ticks.major.size());
    EXPECT_EQ(ticks.labels.front(), "0");
    EXPECT_EQ(ticks.annotation, "×10⁶");
}

TEST(AxisTicks, LogDecadesOrLinearFallback)
{
    const auto decades = makeTicks({
        .range        = Range{.min = 1.0, .max = 1e4},
        .kind         = TickKind::LOG,
        .lengthPx     = 400.0,
        .minSpacingPx = 60.0,
    });
    EXPECT_EQ(decades.labels.front(), "1");
    EXPECT_EQ(decades.labels.back(), "10000");
    EXPECT_EQ(decades.annotation, "");

    const auto narrow = makeTicks({
        .range        = Range{.min = 20.0, .max = 80.0},
        .kind         = TickKind::LOG,
        .lengthPx     = 400.0,
        .minSpacingPx = 60.0,
    });
    EXPECT_GE(narrow.major.size(), 3U);  // linear ticks inside the decade
}

TEST(AxisTicks, SiOnALogAxis)
{
    const auto ticks = makeTicks({
        .range        = Range{.min = 1.0, .max = 1e5},
        .kind         = TickKind::LOG,
        .style        = NumberStyle::SI,
        .lengthPx     = 500.0,
        .minSpacingPx = 60.0,
    });
    EXPECT_EQ(ticks.labels.back(), "100k");
}

TEST(AxisTicks, TimeContextGetsTheZoneName)
{
    const auto ticks = makeTicks({
        .range        = Range{.min = 1.7e9, .max = 1.7e9 + 3600.0},
        .kind         = TickKind::TIME,
        .lengthPx     = 600.0,
        .minSpacingPx = 60.0,
        .zoneName     = "UTC",
    });
    EXPECT_EQ(ticks.annotation, "2023-11-14 UTC");
}

TEST(AxisTicks, ReadoutsOfEachKind)
{
    EXPECT_EQ(formatReadout(2.71828, 0.01, TickKind::LINEAR, NumberStyle::AUTO), "2.72");
    EXPECT_EQ(formatReadout(2500.0, 1.0, TickKind::LOG, NumberStyle::SI), "2.500k");
    EXPECT_EQ(formatReadout(1774656000.0 + 61.0, 1.0, TickKind::TIME, NumberStyle::AUTO),
              "2026-03-28 00:01:01");
    const std::vector<std::string> names{"Nominal", "Gusty", "Engine-out"};
    EXPECT_EQ(formatReadout(1.3, 0.01, TickKind::CATEGORY, NumberStyle::AUTO, {}, names), "Gusty");
    EXPECT_EQ(formatReadout(-0.4, 0.01, TickKind::CATEGORY, NumberStyle::AUTO, {}, names),
              "Nominal");
    // Away from every category: the number.
    EXPECT_EQ(formatReadout(4.0, 0.01, TickKind::CATEGORY, NumberStyle::AUTO, {}, names), "4.00");
}

TEST(AxisTicks, ATickAtEachCategory)
{
    const std::vector<std::string> names{"A", "B", "C", "D"};
    const AxisTicks                ticks = makeTicks({
        .range        = {.min = -0.5, .max = 3.5},
        .kind         = TickKind::CATEGORY,
        .lengthPx     = 400.0,
        .minSpacingPx = 40.0,
        .categories   = names,
    });
    EXPECT_EQ(ticks.major, (std::vector<double>{0.0, 1.0, 2.0, 3.0}));
    EXPECT_EQ(ticks.labels, names);
    EXPECT_TRUE(ticks.minor.empty());
    EXPECT_TRUE(ticks.annotation.empty());
}

TEST(AxisTicks, CrowdedCategoriesAreThinned)
{
    std::vector<std::string> names;
    names.reserve(100);
    for (int i = 0; i < 100; ++i)
    {
        names.push_back(std::to_string(i));
    }
    // 100 categories in 300 pixels, at least 20 pixels apart: every 7th.
    const AxisTicks ticks = makeTicks({
        .range        = {.min = -0.5, .max = 99.5},
        .kind         = TickKind::CATEGORY,
        .lengthPx     = 300.0,
        .minSpacingPx = 20.0,
        .categories   = names,
    });
    ASSERT_GE(ticks.major.size(), 2U);
    EXPECT_EQ(ticks.major[0], 0.0);
    EXPECT_EQ(ticks.major[1], 7.0);
    EXPECT_EQ(ticks.labels[1], "7");
    // Panned, the same ones keep their ticks; none outside the categories.
    const AxisTicks panned = makeTicks({
        .range        = {.min = 10.0, .max = 110.0},
        .kind         = TickKind::CATEGORY,
        .lengthPx     = 300.0,
        .minSpacingPx = 20.0,
        .categories   = names,
    });
    EXPECT_EQ(panned.major.front(), 14.0);
    EXPECT_LE(panned.major.back(), 99.0);
}

}  // namespace
