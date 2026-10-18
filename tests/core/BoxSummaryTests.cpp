#include "core/BoxSummary.h"

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::BoxSummary;
using rocketplot::core::summarize;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

TEST(BoxSummary, QuartilesAndWhiskers)
{
    const BoxSummary box = summarize(std::vector<double>{9, 1, 8, 2, 7, 3, 6, 4, 5}, 1.5, 2.0);
    EXPECT_DOUBLE_EQ(box.q1, 3.0);
    EXPECT_DOUBLE_EQ(box.median, 5.0);
    EXPECT_DOUBLE_EQ(box.q3, 7.0);
    EXPECT_DOUBLE_EQ(box.lowerWhisker, 1.0);
    EXPECT_DOUBLE_EQ(box.upperWhisker, 9.0);
    EXPECT_DOUBLE_EQ(box.mean.value_or(kNaN), 5.0);
    EXPECT_TRUE(box.outliers.empty());
    EXPECT_TRUE(box.farOuts.empty());
}

TEST(BoxSummary, OutliersAndFarOuts)
{
    // 1 to 9, 17 and 100: the quartiles are 3.5 and 8.5, so the whiskers end at 16 (1.5 times
    // the box of 5 above it) and the far-out fence is at 18.5.
    const BoxSummary box =
        summarize(std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 17, 100}, 1.5, 2.0);
    EXPECT_DOUBLE_EQ(box.q1, 3.5);
    EXPECT_DOUBLE_EQ(box.q3, 8.5);
    EXPECT_DOUBLE_EQ(box.upperWhisker, 9.0);
    EXPECT_EQ(box.outliers, (std::vector<double>{17.0}));
    EXPECT_EQ(box.farOuts, (std::vector<double>{100.0}));
    // Tukey's outer fences, at 3: 100 is still far out, 17 an outlier.
    const BoxSummary tukey =
        summarize(std::vector<double>{1, 2, 3, 4, 5, 6, 7, 8, 9, 17, 100}, 1.5, 3.0);
    EXPECT_EQ(tukey.outliers, (std::vector<double>{17.0}));
    EXPECT_EQ(tukey.farOuts, (std::vector<double>{100.0}));
}

TEST(BoxSummary, NaNIsLeftOut)
{
    const BoxSummary box = summarize(std::vector<double>{kNaN, 2.0, 4.0, kNaN}, 1.5, 2.0);
    EXPECT_DOUBLE_EQ(box.median, 3.0);
    EXPECT_DOUBLE_EQ(box.mean.value_or(kNaN), 3.0);
    const BoxSummary none = summarize(std::vector<double>{kNaN}, 1.5, 2.0);
    EXPECT_TRUE(std::isnan(none.median));
    EXPECT_FALSE(none.mean);
}

TEST(BoxSummary, OneValue)
{
    const BoxSummary box = summarize(std::vector<double>{4.0}, 1.5, 2.0);
    EXPECT_EQ(box.lowerWhisker, 4.0);
    EXPECT_EQ(box.q1, 4.0);
    EXPECT_EQ(box.upperWhisker, 4.0);
}

}  // namespace
