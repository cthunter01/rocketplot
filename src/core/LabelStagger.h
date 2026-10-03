#pragma once

#include <span>
#include <vector>

namespace rocketplot::core
{

/// The horizontal extent of a label, in pixels.
struct LabelSpan
{
    double left  = 0.0;
    double right = 0.0;
};

/// The row of a label that no row has room for.
inline constexpr int kNoRow = -1;

/// Puts labels that sit side by side along an axis into rows, so that labels in the same row are
/// at least @p gap apart: each label, from left to right, goes into the first row it fits in (which
/// uses as few rows as possible). Returns the row of each label, 0 being the first; kNoRow for a
/// label that fits in none of @p maxRows rows.
[[nodiscard]] std::vector<int> staggerLabels(std::span<const LabelSpan> labels, double gap,
                                             int maxRows);

}  // namespace rocketplot::core
