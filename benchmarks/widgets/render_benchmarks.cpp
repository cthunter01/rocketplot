// What a frame costs: a plot of 1600 x 900 pixels drawn from scratch (layout, decimation and
// painting), as the widget does after a pan or zoom or when data arrives. 60 frames a second leave
// 16 ms for one.

#include <QColor>
#include <QImage>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/ExportOptions.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;

constexpr int         kWidth        = 1600;
constexpr int         kHeight       = 900;
constexpr std::size_t kLivePoints   = 100'000;  // in each live channel before the measuring starts
constexpr std::size_t kLiveBatch    = 100;      // points that arrive per channel and frame
constexpr int         kLiveChannels = 3;

// One frame of @p plot at a device pixel ratio of 1, per iteration.
void renderFrames(benchmark::State& state, const PlotWidget& plot)
{
    const rocketplot::ExportOptions frame{.size = {kWidth, kHeight}, .dpi = 96.0};
    while (state.KeepRunning())
    {
        QImage image = plot.renderToImage(frame);
        benchmark::DoNotOptimize(image);
    }
}

// A plot without data: what every frame costs before any series is drawn (the image, the
// layout, the axes with their labels, the grid).
void empty(benchmark::State& state)
{
    PlotWidget plot;
    plot.setTitle(QStringLiteral("Title"));
    plot.xAxis()->setLabel(QStringLiteral("Time (s)"));
    plot.yAxis()->setLabel(QStringLiteral("Value"));
    renderFrames(state, plot);
}
BENCHMARK(empty)->Name("Render/Empty")->Unit(benchmark::kMillisecond);

// Lines of noisy data on one time base: the case the library is built for.
void lines(benchmark::State& state)
{
    const auto                count = static_cast<std::size_t>(state.range(1));
    const std::vector<double> time  = rocketplot::bench::ramp(count);
    PlotWidget                plot;
    for (std::int64_t i = 0; i < state.range(0); ++i)
    {
        plot.addLine(time, rocketplot::bench::walk(count, static_cast<std::uint64_t>(i) + 1),
                     QStringLiteral("Channel %1").arg(i + 1));
    }
    renderFrames(state, plot);
}
BENCHMARK(lines)
    ->Name("Render/Lines")
    ->ArgNames({"series", "points"})
    ->Args({1, 1'000'000})
    ->Args({1, 10'000'000})
    ->Args({10, 1'000'000})
    ->Unit(benchmark::kMillisecond);

// Zoomed in until the points are apart: few points, drawn one by one with their markers.
void zoomedIn(benchmark::State& state)
{
    const std::size_t count = 1'000'000;
    PlotWidget        plot;
    plot.addLine(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1))
        ->setMarker(rocketplot::Marker::CIRCLE);
    const double middle = static_cast<double>(count) / 2.0;
    plot.xAxis()->setRange(middle, middle + static_cast<double>(state.range(0)));
    plot.yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    renderFrames(state, plot);
}
BENCHMARK(zoomedIn)
    ->Name("Render/ZoomedIn")
    ->ArgName("points_in_view")
    ->Arg(100)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

// A line that isn't sorted by x: no pyramid, every point visited.
void unsortedLine(benchmark::State& state)
{
    rocketplot::bench::Curve curve =
        rocketplot::bench::lissajous(static_cast<std::size_t>(state.range(0)));
    PlotWidget plot;
    plot.addLine(std::move(curve.x), std::move(curve.y));
    renderFrames(state, plot);
}
BENCHMARK(unsortedLine)
    ->Name("Render/UnsortedLine")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);

void scatter(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    PlotWidget plot;
    plot.addScatter(rocketplot::bench::uniform(count, 0.0, 1.0, 1),
                    rocketplot::bench::uniform(count, 0.0, 1.0, 2));
    renderFrames(state, plot);
}
BENCHMARK(scatter)
    ->Name("Render/Scatter")
    ->ArgName("points")
    ->Arg(10'000)
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);

// Markers with a size and a color each.
void styledScatter(benchmark::State& state)
{
    const auto                 count = static_cast<std::size_t>(state.range(0));
    PlotWidget                 plot;
    rocketplot::ScatterSeries* points =
        plot.addScatter(rocketplot::bench::uniform(count, 0.0, 1.0, 1),
                        rocketplot::bench::uniform(count, 0.0, 1.0, 2));
    points->setSizes(rocketplot::bench::uniform(count, 3.0, 14.0, 3));
    const std::vector<double> shade = rocketplot::bench::uniform(count, 0.0, 255.0, 4);
    std::vector<QColor>       colors;
    colors.reserve(count);
    for (const double value : shade)
    {
        colors.emplace_back(static_cast<int>(value), 80, 255 - static_cast<int>(value));
    }
    points->setColors(colors);
    renderFrames(state, plot);
}
BENCHMARK(styledScatter)
    ->Name("Render/StyledScatter")
    ->ArgName("points")
    ->Arg(100'000)
    ->Unit(benchmark::kMillisecond);

// A line with an uncertainty band around it.
void errorBand(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    PlotWidget plot;
    plot.addLine(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1))
        ->setYErrors(rocketplot::bench::uniform(count, 0.5, 5.0, 2));
    renderFrames(state, plot);
}
BENCHMARK(errorBand)
    ->Name("Render/ErrorBand")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMillisecond);

// A grid of plots drawn as one figure: each plot is laid out twice, once to find the margins
// its row and column line up by.
void grid(benchmark::State& state)
{
    const auto                 count = static_cast<std::size_t>(state.range(2));
    const std::vector<double>  time  = rocketplot::bench::ramp(count);
    const rocketplot::PlotGrid plots(static_cast<int>(state.range(0)),
                                     static_cast<int>(state.range(1)));
    std::uint64_t              seed = 0;
    for (PlotWidget* plot : plots.plots())
    {
        plot->addLine(time, rocketplot::bench::walk(count, ++seed));
    }
    const rocketplot::ExportOptions frame{.size = {kWidth, kHeight}, .dpi = 96.0};
    while (state.KeepRunning())
    {
        QImage image = plots.renderToImage(frame);
        benchmark::DoNotOptimize(image);
    }
}
BENCHMARK(grid)
    ->Name("Render/Grid")
    ->ArgNames({"rows", "columns", "points"})
    ->Args({3, 1, 100'000})
    ->Args({3, 2, 100'000})
    ->Unit(benchmark::kMillisecond);

// A strip chart: each frame, a batch of points arrives on every channel, the view moves on to
// the newest stretch, and the plot is drawn.
void liveFrame(benchmark::State& state)
{
    PlotWidget plot;
    plot.xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
    plot.xAxis()->setFollowWindow(static_cast<double>(kLivePoints) / 10.0);
    plot.yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    const std::vector<double>            time = rocketplot::bench::ramp(kLivePoints);
    std::vector<rocketplot::LineSeries*> channels;
    channels.reserve(kLiveChannels);
    for (int channel = 0; channel < kLiveChannels; ++channel)
    {
        channels.push_back(plot.addLine(
            time, rocketplot::bench::walk(kLivePoints, static_cast<std::uint64_t>(channel) + 1)));
    }
    // What arrives: the same batch of values each frame, at times that go on.
    const std::vector<double>       values = rocketplot::bench::walk(kLiveBatch, 9);
    std::vector<double>             times(kLiveBatch);
    auto                            next = static_cast<double>(kLivePoints);
    const rocketplot::ExportOptions frame{.size = {kWidth, kHeight}, .dpi = 96.0};
    while (state.KeepRunning())
    {
        for (double& at : times)
        {
            at = next;
            next += 1.0;
        }
        for (rocketplot::LineSeries* channel : channels)
        {
            channel->append(times, values);
        }
        QImage image = plot.renderToImage(frame);
        benchmark::DoNotOptimize(image);
    }
}
BENCHMARK(liveFrame)->Name("Render/LiveFrame")->Unit(benchmark::kMillisecond);

// A plot whose title, axis labels and eight legend names are the same text with and without math
// between $ signs: what the formulas cost a frame once they are typeset (they are cached).
void labelled(PlotWidget& plot, bool math)
{
    const auto text = [math](const char* plain, const char* formula) {
        return QString::fromUtf8(math ? formula : plain);
    };
    plot.setTitle(text("Drag F = rho v^2 Cd A / 2", R"(Drag $F_d = \tfrac{1}{2}\rho v^2 C_d A$)"));
    plot.xAxis()->setLabel(text("Time t - t0 (s)", "Time $t - t_0$ (s)"));
    plot.yAxis()->setLabel(text("Speed vz (m/s)", "Speed $v_z$ (m/s)"));
    const std::vector<double> x = rocketplot::bench::ramp(10);
    for (int i = 0; i < 8; ++i)
    {
        plot.addLine(
            x, rocketplot::bench::walk(10, static_cast<std::uint64_t>(i) + 1),
            math ? QStringLiteral("$x_{%1}^2$").arg(i) : QStringLiteral("x%1 squared").arg(i));
    }
}

void plainLabels(benchmark::State& state)
{
    PlotWidget plot;
    labelled(plot, false);
    renderFrames(state, plot);
}
BENCHMARK(plainLabels)->Name("Render/PlainLabels")->Unit(benchmark::kMillisecond);

void mathLabels(benchmark::State& state)
{
    PlotWidget plot;
    labelled(plot, true);
    renderFrames(state, plot);
}
BENCHMARK(mathLabels)->Name("Render/MathLabels")->Unit(benchmark::kMillisecond);

// A new formula every frame: parsing, typesetting and outlining it, not only drawing it.
void newFormula(benchmark::State& state)
{
    PlotWidget plot;
    labelled(plot, true);
    const rocketplot::ExportOptions frame{.size = {kWidth, kHeight}, .dpi = 96.0};
    int                             n = 0;
    while (state.KeepRunning())
    {
        plot.setTitle(QStringLiteral(R"(Frame $\sum_{i=1}^{%1} \frac{x_i^2}{\sqrt{n}}$)").arg(++n));
        QImage image = plot.renderToImage(frame);
        benchmark::DoNotOptimize(image);
    }
}
BENCHMARK(newFormula)->Name("Render/NewFormula")->Unit(benchmark::kMillisecond);

}  // namespace
