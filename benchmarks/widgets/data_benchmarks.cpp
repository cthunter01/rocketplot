// What it costs to give a plot its data, and to take its data and settings out again.

#include <QImage>
#include <QJsonObject>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"

namespace
{

using rocketplot::PlotWidget;

constexpr int kStateSeries = 10;

// How the data gets into a series.
enum class Handover : std::uint8_t
{
    COPY,  // addLine(x, y): the usual way
    MOVE,  // addLine(std::move(x), std::move(y)): the series takes the vectors over
    VIEW,  // addLineView(x, y): the series reads the caller's memory
};

// A line added to a plot: the data handed over, and what the series works out about it (sorted
// or not, bounds, min/max pyramid). The plot isn't drawn.
void addLine(benchmark::State& state, Handover handover)
{
    const auto                count = static_cast<std::size_t>(state.range(0));
    const std::vector<double> x     = rocketplot::bench::ramp(count);
    const std::vector<double> y     = rocketplot::bench::walk(count, 1);
    while (state.KeepRunning())
    {
        PlotWidget plot;
        switch (handover)
        {
            case Handover::COPY:
                plot.addLine(x, y);
                break;
            case Handover::MOVE:
            {
                state.PauseTiming();  // the copies to move from aren't what is measured
                std::vector<double> ownX = x;
                std::vector<double> ownY = y;
                state.ResumeTiming();
                plot.addLine(std::move(ownX), std::move(ownY));
                break;
            }
            case Handover::VIEW:
                plot.addLineView(std::span<const double>(x), std::span<const double>(y));
                break;
        }
        benchmark::DoNotOptimize(plot);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK_CAPTURE(addLine, copy, Handover::COPY)
    ->Name("Data/AddLine/Copy")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(addLine, move, Handover::MOVE)
    ->Name("Data/AddLine/Move")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(addLine, view, Handover::VIEW)
    ->Name("Data/AddLine/View")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);

// The data in view as CSV text: three channels on one time base.
void toCsv(benchmark::State& state)
{
    const auto                count = static_cast<std::size_t>(state.range(0));
    const std::vector<double> time  = rocketplot::bench::ramp(count);
    PlotWidget                plot;
    for (int channel = 0; channel < 3; ++channel)
    {
        plot.addLine(time, rocketplot::bench::walk(count, static_cast<std::uint64_t>(channel) + 1),
                     QStringLiteral("Channel %1").arg(channel + 1));
    }
    while (state.KeepRunning())
    {
        QString text = plot.toCsv();
        benchmark::DoNotOptimize(text);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(toCsv)->Name("Data/ToCsv")->ArgName("rows")->Arg(100'000)->Unit(benchmark::kMillisecond);

// A plot's settings saved as JSON and applied again.
void saveAndRestoreState(benchmark::State& state)
{
    const std::vector<double> time = rocketplot::bench::ramp(1000);
    PlotWidget                plot;
    for (int channel = 0; channel < kStateSeries; ++channel)
    {
        plot.addLine(time, rocketplot::bench::walk(1000, static_cast<std::uint64_t>(channel) + 1),
                     QStringLiteral("Channel %1").arg(channel + 1));
    }
    while (state.KeepRunning())
    {
        const QJsonObject saved    = plot.saveState();
        bool              restored = plot.restoreState(saved);
        benchmark::DoNotOptimize(restored);
    }
}
BENCHMARK(saveAndRestoreState)->Name("Data/SaveAndRestoreState")->Unit(benchmark::kMicrosecond);

}  // namespace
