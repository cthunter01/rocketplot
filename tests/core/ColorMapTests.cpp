#include "core/ColorMap.h"

#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::ColorTable;
using rocketplot::core::GradientStop;
using rocketplot::core::Rgba;
using rocketplot::core::tableAt;

constexpr Rgba rgb(int r, int g, int b)
{
    return {.r = static_cast<std::uint8_t>(r),
            .g = static_cast<std::uint8_t>(g),
            .b = static_cast<std::uint8_t>(b)};
}

TEST(ColorMap, ViridisEnds)
{
    EXPECT_EQ(tableAt(ColorTable::VIRIDIS, 0.0), rgb(0x44, 0x01, 0x54));
    EXPECT_EQ(tableAt(ColorTable::VIRIDIS, 1.0), rgb(0xfd, 0xe7, 0x25));
}

TEST(ColorMap, BlueRedAndCoolWarm)
{
    EXPECT_EQ(tableAt(ColorTable::BLUE_RED, 0.0), rgb(0, 0, 255));
    EXPECT_EQ(tableAt(ColorTable::BLUE_RED, 0.5), rgb(255, 255, 255));
    EXPECT_EQ(tableAt(ColorTable::BLUE_RED, 1.0), rgb(255, 0, 0));
    EXPECT_EQ(tableAt(ColorTable::COOL_WARM, 0.0), rgb(59, 76, 192));
    EXPECT_EQ(tableAt(ColorTable::COOL_WARM, 1.0), rgb(180, 4, 38));
    const Rgba middle = tableAt(ColorTable::COOL_WARM, 0.5);
    EXPECT_NEAR(middle.r, 221, 2);
    EXPECT_NEAR(middle.g, 221, 2);
    EXPECT_NEAR(middle.b, 221, 2);
    EXPECT_EQ(tableAt(ColorTable::GRAY, 1.0), rgb(255, 255, 255));
}

TEST(ColorMap, ValuesOutsideAreClamped)
{
    EXPECT_EQ(tableAt(ColorTable::MAGMA, -3.0), tableAt(ColorTable::MAGMA, 0.0));
    EXPECT_EQ(tableAt(ColorTable::MAGMA, 7.0), tableAt(ColorTable::MAGMA, 1.0));
    EXPECT_EQ(tableAt(ColorTable::MAGMA, std::numeric_limits<double>::quiet_NaN()),
              tableAt(ColorTable::MAGMA, 0.0));
}

TEST(ColorMap, Gradients)
{
    using rocketplot::core::gradientAt;
    const std::vector<GradientStop> stops{{.position = 0.2, .color = rgb(0, 0, 0)},
                                          {.position = 0.6, .color = rgb(200, 100, 0)},
                                          {.position = 1.0, .color = rgb(200, 100, 200)}};
    EXPECT_EQ(gradientAt(stops, 0.0), rgb(0, 0, 0));  // the first below it
    EXPECT_EQ(gradientAt(stops, 0.4), rgb(100, 50, 0));
    EXPECT_EQ(gradientAt(stops, 0.8), rgb(200, 100, 100));
    EXPECT_EQ(gradientAt({}, 0.5), Rgba{});
}

TEST(ColorMap, LookupTables)
{
    using rocketplot::core::buildLut;
    using rocketplot::core::lutAt;
    const rocketplot::core::Lut lut = buildLut(ColorTable::VIRIDIS);
    EXPECT_EQ(lutAt(lut, 0.0), tableAt(ColorTable::VIRIDIS, 0.0));
    EXPECT_EQ(lutAt(lut, 1.0), tableAt(ColorTable::VIRIDIS, 1.0));
    EXPECT_EQ(lutAt(lut, 2.0), lutAt(lut, 1.0));
}

}  // namespace
