#include "rocketplot/ScatterSeries.h"

#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

ScatterSeries::ScatterSeries(PlotWidget* plot) : Series(plot, Marker::CIRCLE) { }

ScatterSeries::~ScatterSeries() = default;

}  // namespace rocketplot
