#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

// What it costs to hand a series its data, and to ask it what autoscale asks.

namespace
{

using rocketplot::Range;
using rocketplot::core::SeriesData;

constexpr std::size_t kAppended = 1'000'000;  // points appended per iteration

// What a series works out when it is given data: whether x is sorted, the bounds, and the min/max
// pyramid. A view, so that no copying is measured.
void index(benchmark::State& state)
{
    const auto                count = static_cast<std::size_t>(state.range(0));
    const std::vector<double> x     = rocketplot::bench::ramp(count);
    const std::vector<double> y     = rocketplot::bench::walk(count, 1);
    while (state.KeepRunning())
    {
        SeriesData data;
        data.setView(x, y);
        benchmark::DoNotOptimize(data);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(index)
    ->Name("SeriesData/Index")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMillisecond);

// Live data: a million points arriving a batch at a time, the derived data kept up to date as
// they come.
void append(benchmark::State& state)
{
    const auto                batch = static_cast<std::size_t>(state.range(0));
    const std::vector<double> x     = rocketplot::bench::ramp(kAppended);
    const std::vector<double> y     = rocketplot::bench::walk(kAppended, 1);
    while (state.KeepRunning())
    {
        SeriesData data;
        for (std::size_t first = 0; first < kAppended; first += batch)
        {
            const std::size_t count = std::min(batch, kAppended - first);
            data.append(std::span(x).subspan(first, count), std::span(y).subspan(first, count));
        }
        benchmark::DoNotOptimize(data);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kAppended));
}
BENCHMARK(append)
    ->Name("SeriesData/Append")
    ->ArgName("batch")
    ->Arg(1)
    ->Arg(100)
    ->Arg(10'000)
    ->Unit(benchmark::kMillisecond);

// What a y axis that fits the visible data asks on every pan and zoom: the bounds of the points
// in an x range, read from the pyramid.
void boundsInView(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1));
    const auto  last = static_cast<double>(count - 1);
    const Range view{.min = last * 0.45, .max = last * 0.55};
    while (state.KeepRunning())
    {
        Range bounds = data.yBoundsWithin(view, false);
        benchmark::DoNotOptimize(bounds);
    }
}
BENCHMARK(boundsInView)
    ->Name("SeriesData/BoundsInView")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kNanosecond);

}  // namespace
