// The text a plot produces: tick labels on every frame, and the data as CSV.

#include <cstddef>
#include <string_view>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "core/AxisTicks.h"
#include "core/CsvWriter.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::AxisTicks;
using rocketplot::core::CsvSeries;
using rocketplot::core::CsvTable;
using rocketplot::core::SeriesData;
using rocketplot::core::TickKind;
using rocketplot::core::TickRequest;

constexpr double kAxisLength  = 1600.0;
constexpr double kTickSpacing = 80.0;
constexpr double kOneDay      = 86'400.0;
constexpr double kNow         = 1.79e9;  // seconds since the epoch: late 2026

// The ticks and labels of one axis: made anew for every axis on every frame.
void ticks(benchmark::State& state, TickKind kind, Range range)
{
    const TickRequest request{
        .range        = range,
        .kind         = kind,
        .lengthPx     = kAxisLength,
        .minSpacingPx = kTickSpacing,
    };
    while (state.KeepRunning())
    {
        AxisTicks made = rocketplot::core::makeTicks(request);
        benchmark::DoNotOptimize(made);
    }
}
BENCHMARK_CAPTURE(ticks, linear, TickKind::LINEAR, Range{.min = -3.7, .max = 12.2})
    ->Name("AxisTicks/Linear");
BENCHMARK_CAPTURE(ticks, log, TickKind::LOG, Range{.min = 0.02, .max = 5.0e6})
    ->Name("AxisTicks/Log");
BENCHMARK_CAPTURE(ticks, time, TickKind::TIME, Range{.min = kNow, .max = kNow + (3.0 * kOneDay)})
    ->Name("AxisTicks/Time");

// Three channels on one time base, written as CSV.
void csv(benchmark::State& state)
{
    const auto                count = static_cast<std::size_t>(state.range(0));
    const std::vector<double> time  = rocketplot::bench::ramp(count);
    std::vector<SeriesData>   data(3);
    std::vector<CsvSeries>    series;
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        data[i].setOwned(time, rocketplot::bench::walk(count, i + 1));
        series.push_back({.name = "channel", .data = &data[i]});
    }
    const CsvTable table{.series = series, .xRange = data.front().xBounds(), .xName = "time"};
    while (state.KeepRunning())
    {
        std::size_t written = 0;
        rocketplot::core::writeCsv(table,
                                   [&written](std::string_view text) { written += text.size(); });
        benchmark::DoNotOptimize(written);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(csv)
    ->Name("CsvWriter/Write")
    ->ArgName("rows")
    ->Arg(100'000)
    ->Unit(benchmark::kMillisecond);

}  // namespace
