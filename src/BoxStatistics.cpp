#include "rocketplot/BoxStatistics.h"

#include <span>
#include <utility>

#include "core/BoxSummary.h"

namespace rocketplot
{

BoxStatistics BoxStatistics::fromSpan(std::span<const double> values, double whiskerIqr,
                                      double farOutIqr)
{
    core::BoxSummary summary = core::summarize(values, whiskerIqr, farOutIqr);
    return {
        .lowerWhisker = summary.lowerWhisker,
        .q1           = summary.q1,
        .median       = summary.median,
        .q3           = summary.q3,
        .upperWhisker = summary.upperWhisker,
        .mean         = summary.mean,
        .outliers     = std::move(summary.outliers),
        .farOuts      = std::move(summary.farOuts),
    };
}

}  // namespace rocketplot
