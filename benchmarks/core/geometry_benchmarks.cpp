// The outlines that are filled instead of stroked: a thick line through dense data, and the band
// between the ends of the errors.

#include <cstddef>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/ErrorData.h"
#include "core/ErrorGeometry.h"
#include "core/LineBand.h"
#include "core/SeriesData.h"

namespace
{

using rocketplot::core::AxisMapping;
using rocketplot::core::ColumnGrid;
using rocketplot::core::ErrorBar;
using rocketplot::core::ErrorData;
using rocketplot::core::PixelBox;
using rocketplot::core::Polyline;
using rocketplot::core::SeriesData;

constexpr double      kWidth   = 1600.0;
constexpr double      kHeight  = 900.0;
constexpr std::size_t kColumns = 1600;
constexpr double      kOutside = 16.0;  // how far past the plot the band is cut off
constexpr double      kBarCell = 6.0;   // one error bar per cell of this many pixels

// A noisy series of @p count points with an error on each, and mappings that show all of it.
struct Measured
{
    SeriesData  data;
    ErrorData   errors;
    AxisMapping x;
    AxisMapping y;

    explicit Measured(std::size_t count)
    {
        data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1));
        const std::vector<double> error = rocketplot::bench::uniform(count, 0.5, 5.0, 2);
        errors.setY(data, error, error);
        x = AxisMapping(data.xBounds(), 0.0, kWidth);
        y = AxisMapping(data.yBounds(), kHeight, 0.0);
    }
};

// The outline of a 2-pixel line through a million points, from its decimated points.
void lineOutline(benchmark::State& state)
{
    const Measured measured(static_cast<std::size_t>(state.range(0)));
    Polyline       line;
    rocketplot::core::decimateLine(measured.data, measured.x, measured.y, 1.0, line);
    const ColumnGrid           grid{.left = 0.0, .width = 1.0, .count = kColumns};
    rocketplot::core::LineBand band;
    Polyline                   polygons;
    while (state.KeepRunning())
    {
        polygons.clear();
        band.outline(line, grid, 1.0, polygons);
        benchmark::DoNotOptimize(polygons);
    }
    state.counters["polygons"] = static_cast<double>(polygons.runCount());
}
BENCHMARK(lineOutline)
    ->Name("LineBand/Outline")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMicrosecond);

void errorBand(benchmark::State& state)
{
    const Measured   measured(static_cast<std::size_t>(state.range(0)));
    const ColumnGrid grid{.left = 0.0, .width = 1.0, .count = kColumns};
    Polyline         polygons;
    while (state.KeepRunning())
    {
        polygons.clear();
        std::size_t visited = rocketplot::core::decimateErrorBand(
            measured.data, measured.errors, measured.x, measured.y, grid, -kOutside,
            kHeight + kOutside, polygons);
        benchmark::DoNotOptimize(visited);
    }
}
BENCHMARK(errorBand)
    ->Name("ErrorGeometry/Band")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMicrosecond);

void errorBars(benchmark::State& state)
{
    const Measured        measured(static_cast<std::size_t>(state.range(0)));
    const PixelBox        box{.left = 0.0, .top = 0.0, .right = kWidth, .bottom = kHeight};
    std::vector<ErrorBar> bars;
    while (state.KeepRunning())
    {
        bars.clear();
        std::size_t visited = rocketplot::core::collectErrorBars(
            measured.data, measured.errors, measured.x, measured.y, box, kBarCell, true, bars);
        benchmark::DoNotOptimize(visited);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(errorBars)
    ->Name("ErrorGeometry/Bars")
    ->ArgName("points")
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
