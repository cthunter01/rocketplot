#include "core/LabelStagger.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace rocketplot::core
{

std::vector<int> staggerLabels(std::span<const LabelSpan> labels, double gap, int maxRows)
{
    std::vector<int> rows(labels.size(), kNoRow);
    // Left to right; labels starting at the same place keep their order.
    std::vector<std::size_t> order(labels.size());
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::ranges::stable_sort(
        order, [&](std::size_t a, std::size_t b) { return labels[a].left < labels[b].left; });

    std::vector<double> rowEnds;  // the right edge of the last label in each row
    for (const std::size_t index : order)
    {
        const LabelSpan& label = labels[index];
        const auto       free =
            std::ranges::find_if(rowEnds, [&](double end) { return end + gap <= label.left; });
        if (free != rowEnds.end())
        {
            *free       = label.right;
            rows[index] = static_cast<int>(free - rowEnds.begin());
        }
        else if (std::cmp_less(rowEnds.size(), maxRows))
        {
            rows[index] = static_cast<int>(rowEnds.size());
            rowEnds.push_back(label.right);
        }
    }
    return rows;
}

}  // namespace rocketplot::core
