#pragma once

#include <optional>
#include <span>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/export.h"

namespace rocketplot
{

/// What a box plot shows of a set of values (PlotWidget::addBox()): the box from the first
/// quartile to the third with the median across it, whiskers out to the lower and upper whisker,
/// and the values beyond those, as outliers and, further out, far-outs. The mean is shown when it
/// is given (and BoxSeries::setMeanVisible() asks for it).
///
/// fromValues() works it out from the values; a caller with a rule of its own fills the numbers in
/// itself. A plain value type, like Range: no Qt.
struct ROCKETPLOT_EXPORT BoxStatistics
{
    double                lowerWhisker = 0.0;
    double                q1           = 0.0;
    double                median       = 0.0;
    double                q3           = 0.0;
    double                upperWhisker = 0.0;
    std::optional<double> mean;
    std::vector<double>   outliers;
    std::vector<double>   farOuts;

    /// The statistics of @p values: the quartiles and the median as R's and NumPy's default
    /// quantiles have them (interpolated between the two nearest values); whiskers out to the
    /// furthest values within @p whiskerIqr times the box's height (the interquartile range) of
    /// it; values beyond those up to @p farOutIqr times it outliers, and those beyond that
    /// far-outs; the mean of them all. NaN and infinities are left out; with no values left every
    /// number is NaN, which BoxSeries::setStatistics() rejects.
    ///
    /// The defaults are JFreeChart's fences; Tukey's outer fences are at 3, as in
    /// fromValues(values, 1.5, 3.0).
    template <NumericRange V>
    [[nodiscard]] static BoxStatistics fromValues(const V& values, double whiskerIqr = 1.5,
                                                  double farOutIqr = 2.0)
    {
        const std::vector<double> copy = detail::toDoubleVector(values);
        return fromSpan(copy, whiskerIqr, farOutIqr);
    }
    [[nodiscard]] static BoxStatistics fromSpan(std::span<const double> values,
                                                double whiskerIqr = 1.5, double farOutIqr = 2.0);

    friend bool operator==(const BoxStatistics&, const BoxStatistics&) = default;
};

}  // namespace rocketplot
