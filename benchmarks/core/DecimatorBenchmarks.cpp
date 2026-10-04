#include <cstddef>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

// What a frame costs before anything is drawn: turning a series into the points to draw, for a
// plot area of 1600 x 900 pixels.

namespace
{

using rocketplot::Range;
using rocketplot::core::AxisMapping;
using rocketplot::core::decimateLine;
using rocketplot::core::decimateScatter;
using rocketplot::core::DecimationResult;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::Polyline;
using rocketplot::core::SeriesData;

constexpr double kWidth      = 1600.0;
constexpr double kHeight     = 900.0;
constexpr double kMarkerCell = 3.0;  // markers closer together than this are drawn as one

struct View
{
    AxisMapping x;
    AxisMapping y;
};

// A view of the middle @p percent of the x range of @p data, and all of its y range.
View viewOf(const SeriesData& data, double percent = 100.0)
{
    const Range  x      = data.xBounds();
    const double center = (x.min + x.max) / 2.0;
    const double half   = (x.max - x.min) * percent / 200.0;
    return {
        .x = AxisMapping(Range{.min = center - half, .max = center + half}, 0.0, kWidth),
        .y = AxisMapping(data.yBounds(), kHeight, 0.0),
    };
}

// A line sorted by x, the usual case: only the points in view are visited, and each pixel column
// is summarized from the min/max pyramid.
void sortedLine(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1));
    const View view = viewOf(data, static_cast<double>(state.range(1)));
    Polyline   line;
    while (state.KeepRunning())
    {
        line.clear();
        DecimationResult result = decimateLine(data, view.x, view.y, 1.0, line);
        benchmark::DoNotOptimize(result);
    }
    state.counters["points_drawn"] = static_cast<double>(line.pointCount());
}
BENCHMARK(sortedLine)
    ->Name("Decimator/SortedLine")
    ->ArgNames({"points", "percent_in_view"})
    ->Args({1'000'000, 100})
    ->Args({10'000'000, 100})
    ->Args({10'000'000, 1})
    ->Unit(benchmark::kMicrosecond);

// The same without an x array: the column boundaries are computed, not searched for.
void uniformLine(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::UniformX{.start = 0.0, .step = 0.001},
                  rocketplot::bench::walk(count, 1));
    const View view = viewOf(data);
    Polyline   line;
    while (state.KeepRunning())
    {
        line.clear();
        DecimationResult result = decimateLine(data, view.x, view.y, 1.0, line);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(uniformLine)
    ->Name("Decimator/UniformLine")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMicrosecond);

// A line that isn't sorted by x: every point is visited.
void unsortedLine(benchmark::State& state)
{
    const auto               count = static_cast<std::size_t>(state.range(0));
    rocketplot::bench::Curve curve = rocketplot::bench::lissajous(count);
    SeriesData               data;
    data.setOwned(std::move(curve.x), std::move(curve.y));
    const View view = viewOf(data);
    Polyline   line;
    while (state.KeepRunning())
    {
        line.clear();
        DecimationResult result = decimateLine(data, view.x, view.y, 1.0, line);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(unsortedLine)
    ->Name("Decimator/UnsortedLine")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMicrosecond);

// A cloud of markers: every point is visited, and one per cell of a few pixels is kept.
void scatter(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::uniform(count, 0.0, 1.0, 1),
                  rocketplot::bench::uniform(count, 0.0, 1.0, 2));
    const View              view = viewOf(data);
    const PixelBox          box{.left = 0.0, .top = 0.0, .right = kWidth, .bottom = kHeight};
    std::vector<PixelPoint> points;
    while (state.KeepRunning())
    {
        points.clear();
        std::size_t visited = decimateScatter(data, view.x, view.y, box, kMarkerCell, points);
        benchmark::DoNotOptimize(visited);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.counters["points_drawn"] = static_cast<double>(points.size());
}
BENCHMARK(scatter)
    ->Name("Decimator/Scatter")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
