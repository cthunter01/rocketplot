#include <cstddef>
#include <optional>
#include <vector>

#include <benchmark/benchmark.h>

#include "BenchmarkData.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/NearestPoint.h"
#include "core/SeriesData.h"

// What it costs to follow the pointer with a crosshair that goes to the data: finding, each time
// the pointer moves, the point of a series nearest to it in a plot area of 1600 x 900 pixels.

namespace
{

using rocketplot::core::AxisMapping;
using rocketplot::core::NearestPoint;
using rocketplot::core::nearestPoint;
using rocketplot::core::PixelBox;
using rocketplot::core::PixelPoint;
using rocketplot::core::SeriesData;
using rocketplot::core::tracedPoint;

constexpr double      kWidth     = 1600.0;
constexpr double      kHeight    = 900.0;
constexpr double      kReach     = 20.0;  // of a snapping crosshair, in pixels
constexpr std::size_t kPositions = 256;
constexpr PixelBox    kBox{.left = 0.0, .top = 0.0, .right = kWidth, .bottom = kHeight};

struct View
{
    AxisMapping x;
    AxisMapping y;
};

// All of @p data in the plot area.
View viewOf(const SeriesData& data)
{
    return {
        .x = AxisMapping(data.xBounds(), 0.0, kWidth),
        .y = AxisMapping(data.yBounds(), kHeight, 0.0),
    };
}

// Places for the pointer, each @p above pixels above a point of @p data picked at random: beside
// the line, where a crosshair is wanted.
std::vector<PixelPoint> positionsNear(const SeriesData& data, const View& view, double above)
{
    std::vector<PixelPoint> positions;
    positions.reserve(kPositions);
    for (const double share : rocketplot::bench::uniform(kPositions, 0.0, 1.0, 3))
    {
        const auto index = static_cast<std::size_t>(share * static_cast<double>(data.size() - 1));
        positions.push_back({
            .x = view.x.toPixel(data.x(index)),
            .y = view.y.toPixel(data.y(index)) - above,
        });
    }
    return positions;
}

// Places for the pointer anywhere in the plot area.
std::vector<PixelPoint> positionsAnywhere()
{
    const std::vector<double> x = rocketplot::bench::uniform(kPositions, 0.0, kWidth, 4);
    const std::vector<double> y = rocketplot::bench::uniform(kPositions, 0.0, kHeight, 5);
    std::vector<PixelPoint>   positions(kPositions);
    for (std::size_t i = 0; i < kPositions; ++i)
    {
        positions[i] = {.x = x[i], .y = y[i]};
    }
    return positions;
}

// One search per iteration, the pointer moving on to the next of @p positions each time.
template <class Find>
void follow(benchmark::State& state, const std::vector<PixelPoint>& positions, Find find)
{
    std::size_t next  = 0;
    std::size_t found = 0;
    while (state.KeepRunning())
    {
        std::optional<NearestPoint> point = find(positions[next]);
        benchmark::DoNotOptimize(point);
        found += point ? 1U : 0U;
        next = (next + 1) % positions.size();
    }
    state.counters["found_percent"] =
        100.0 * static_cast<double>(found) / static_cast<double>(state.iterations());
}

// Snapping to a line sorted by x, the pointer a few pixels from it: thousands of points lie
// within reach, and the pyramid rules nearly all of them out.
void snapToLine(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1));
    const View view = viewOf(data);
    follow(state, positionsNear(data, view, 5.0), [&](PixelPoint position) {
        return nearestPoint(data, view.x, view.y, kBox, position, kReach);
    });
}
BENCHMARK(snapToLine)
    ->Name("NearestPoint/SnapToLine")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMicrosecond);

// The same over data that fills the plot from top to bottom in every pixel column (noise, not a
// line): wherever the pointer is, there are points all around it, and little can be ruled out
// before a point right next to it is found.
void snapToNoise(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::uniform(count, -1.0, 1.0, 2));
    const View view = viewOf(data);
    follow(state, positionsAnywhere(), [&](PixelPoint position) {
        return nearestPoint(data, view.x, view.y, kBox, position, kReach);
    });
}
BENCHMARK(snapToNoise)
    ->Name("NearestPoint/SnapToNoise")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMicrosecond);

// Tracing a line sorted by x, the pointer anywhere above or below it: only the points of the
// pointer's pixel column are candidates.
void traceLine(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    SeriesData data;
    data.setOwned(rocketplot::bench::ramp(count), rocketplot::bench::walk(count, 1));
    const View view = viewOf(data);
    follow(state, positionsAnywhere(),
           [&](PixelPoint position) { return tracedPoint(data, view.x, view.y, kBox, position); });
}
BENCHMARK(traceLine)
    ->Name("NearestPoint/TraceLine")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Arg(10'000'000)
    ->Unit(benchmark::kMicrosecond);

// A curve that isn't sorted by x: every point is visited, each time the pointer moves.
void snapToUnsortedCurve(benchmark::State& state)
{
    const auto                     count = static_cast<std::size_t>(state.range(0));
    const rocketplot::bench::Curve curve = rocketplot::bench::lissajous(count);
    SeriesData                     data;
    data.setView(curve.x, curve.y);
    const View view = viewOf(data);
    follow(state, positionsNear(data, view, 5.0), [&](PixelPoint position) {
        return nearestPoint(data, view.x, view.y, kBox, position, kReach);
    });
}
BENCHMARK(snapToUnsortedCurve)
    ->Name("NearestPoint/SnapToUnsortedCurve")
    ->ArgName("points")
    ->Arg(1'000'000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace
