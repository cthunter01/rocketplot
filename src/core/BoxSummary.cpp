#include "core/BoxSummary.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace rocketplot::core
{

double quantile(std::span<const double> sorted, double share)
{
    if (sorted.empty())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double position   = std::clamp(share, 0.0, 1.0) * static_cast<double>(sorted.size() - 1);
    const auto   below      = static_cast<std::size_t>(std::floor(position));
    const std::size_t above = std::min(below + 1, sorted.size() - 1);
    return sorted[below] +
           ((position - static_cast<double>(below)) * (sorted[above] - sorted[below]));
}

BoxSummary summarize(std::span<const double> values, double whiskerIqr, double farOutIqr)
{
    std::vector<double> sorted;
    sorted.reserve(values.size());
    std::ranges::copy_if(values, std::back_inserter(sorted),
                         [](double value) { return std::isfinite(value); });
    std::ranges::sort(sorted);
    BoxSummary box;
    if (sorted.empty())
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        box.lowerWhisker = box.q1 = box.median = box.q3 = box.upperWhisker = nan;
        return box;
    }
    box.q1     = quantile(sorted, 0.25);
    box.median = quantile(sorted, 0.5);
    box.q3     = quantile(sorted, 0.75);
    box.mean =
        std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(sorted.size());
    const double iqr         = box.q3 - box.q1;
    const double whiskerLow  = box.q1 - (whiskerIqr * iqr);
    const double whiskerHigh = box.q3 + (whiskerIqr * iqr);
    const double farLow      = box.q1 - (std::max(farOutIqr, whiskerIqr) * iqr);
    const double farHigh     = box.q3 + (std::max(farOutIqr, whiskerIqr) * iqr);
    // The whiskers reach the furthest values within their fences, but never into the box.
    box.lowerWhisker = box.q1;
    box.upperWhisker = box.q3;
    for (const double value : sorted)
    {
        if (value >= whiskerLow && value <= whiskerHigh)
        {
            box.lowerWhisker = std::min(box.lowerWhisker, value);
            box.upperWhisker = std::max(box.upperWhisker, value);
        }
        else if (value >= farLow && value <= farHigh)
        {
            box.outliers.push_back(value);
        }
        else
        {
            box.farOuts.push_back(value);
        }
    }
    return box;
}

}  // namespace rocketplot::core
