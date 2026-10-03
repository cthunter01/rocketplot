#include "core/LabelStagger.h"

#include <vector>

#include <gtest/gtest.h>

namespace
{

using rocketplot::core::kNoRow;
using rocketplot::core::LabelSpan;
using rocketplot::core::staggerLabels;

TEST(LabelStagger, LabelsApartShareTheFirstRow)
{
    const std::vector<LabelSpan> labels{{.left = 0.0, .right = 40.0},
                                        {.left = 50.0, .right = 90.0},
                                        {.left = 100.0, .right = 140.0}};
    EXPECT_EQ(staggerLabels(labels, 4.0, 3), (std::vector<int>{0, 0, 0}));
}

TEST(LabelStagger, OverlappingLabelsGoIntoLowerRows)
{
    const std::vector<LabelSpan> labels{{.left = 0.0, .right = 40.0},
                                        {.left = 10.0, .right = 50.0},
                                        {.left = 20.0, .right = 60.0},
                                        {.left = 45.0, .right = 85.0}};
    // The fourth fits after the first again.
    EXPECT_EQ(staggerLabels(labels, 4.0, 3), (std::vector<int>{0, 1, 2, 0}));
}

TEST(LabelStagger, TheGapCounts)
{
    const std::vector<LabelSpan> labels{{.left = 0.0, .right = 40.0},
                                        {.left = 42.0, .right = 80.0}};
    EXPECT_EQ(staggerLabels(labels, 4.0, 2), (std::vector<int>{0, 1}));
    EXPECT_EQ(staggerLabels(labels, 2.0, 2), (std::vector<int>{0, 0}));
}

TEST(LabelStagger, PlacedFromLeftToRightWhateverTheOrderGiven)
{
    const std::vector<LabelSpan> labels{
        {.left = 20.0, .right = 60.0}, {.left = 0.0, .right = 40.0}, {.left = 10.0, .right = 50.0}};
    EXPECT_EQ(staggerLabels(labels, 4.0, 3), (std::vector<int>{2, 0, 1}));
}

TEST(LabelStagger, NoRoomWithinTheRowsAllowed)
{
    const std::vector<LabelSpan> labels{
        {.left = 0.0, .right = 40.0}, {.left = 10.0, .right = 50.0}, {.left = 20.0, .right = 60.0}};
    EXPECT_EQ(staggerLabels(labels, 4.0, 2), (std::vector<int>{0, 1, kNoRow}));
    EXPECT_EQ(staggerLabels(labels, 4.0, 0), (std::vector<int>{kNoRow, kNoRow, kNoRow}));
    EXPECT_TRUE(staggerLabels({}, 4.0, 2).empty());
}

}  // namespace
