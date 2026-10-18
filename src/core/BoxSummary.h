#pragma once

#include <optional>
#include <span>
#include <vector>

namespace rocketplot::core
{

/// What a box and its whiskers show of a set of values.
struct BoxSummary
{
    double                lowerWhisker = 0.0;
    double                q1           = 0.0;
    double                median       = 0.0;
    double                q3           = 0.0;
    double                upperWhisker = 0.0;
    std::optional<double> mean;
    std::vector<double>   outliers;  ///< Beyond the whiskers, within the far-out fences
    std::vector<double>   farOuts;   ///< Beyond the far-out fences
};

/// The value below which @p share of @p sorted lies, interpolated between the two nearest (the
/// quantile R and NumPy call type 7). NaN for no values.
[[nodiscard]] double quantile(std::span<const double> sorted, double share);

/// The box of @p values: the quartiles and the median (quantile()); whiskers out to the furthest
/// values within @p whiskerIqr times the interquartile range of the box; values beyond those, up
/// to @p farOutIqr times it, outliers, and the rest far-outs; the mean of them all. NaN and
/// infinities are left out; with no values left, every number is NaN and there is no mean.
[[nodiscard]] BoxSummary summarize(std::span<const double> values, double whiskerIqr,
                                   double farOutIqr);

}  // namespace rocketplot::core
