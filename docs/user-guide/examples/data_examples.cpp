// Examples of data.md.

#include <QList>
#include <QString>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

// The ways data gets into a series.
void handingDataOver(const QString& /*scratch*/)
{
    PlotWidget  window;
    PlotWidget* plot = &window;
    // [data-copy]
    const std::vector<double> time{0.0, 1.0, 2.0, 3.0};
    const std::vector<float>  pressure{101.3F, 99.8F, 97.1F, 93.6F};  // any type of number
    const std::array<int, 4>  counts{3, 1, 4, 1};
    const QList<double>       voltage{4.9, 5.0, 5.1, 5.0};

    plot->addLine(time, pressure, "Pressure");  // x and y: copied, as double
    plot->addLine(time, voltage, "Voltage");
    plot->addScatter(counts, "Counts");  // y alone: x is 0, 1, 2, ...
    // [/data-copy]

    // [data-move]
    std::vector<double> x(1'000'000);
    std::vector<double> y(1'000'000);
    // ... fill them ...
    plot->addLine(std::move(x), std::move(y), "Moved in");  // the series takes the vectors over
    // [/data-move]

    // [data-view]
    std::vector<double>     samples(48'000);  // a buffer that the application owns and refills
    rocketplot::LineSeries* scope =
        plot->addLineView(rocketplot::UniformX{.start = 0.0, .step = 1.0 / 48'000.0}, samples);

    samples[100] = 0.5;          // after changing the buffer in place ...
    scope->notifyDataChanged();  // ... tell the series, which reads it again
    // [/data-view]

    // [data-replace]
    rocketplot::LineSeries* trace = plot->addLine(time, pressure, "Trace");

    // Other data, same series.
    trace->setData(time, voltage);
    // Or with implicit x.
    trace->setData(rocketplot::UniformX{.start = 0.0, .step = 0.5}, voltage);
    // No points at all.
    trace->clear();
    // [/data-replace]

    // [data-mismatch]
    QString problem;
    try
    {
        plot->addLine(std::vector<double>{0.0, 1.0, 2.0}, std::vector<double>{5.0, 6.0});
    }
    catch (const std::invalid_argument& error)
    {
        // x and y must have the same number of values: nothing was added.
        problem = error.what();
    }
    // [/data-mismatch]
}

void readingDataBack(const QString& /*scratch*/)
{
    PlotWidget                window;
    PlotWidget*               plot   = &window;
    const Ascent              ascent = sampleAscent();
    const std::vector<double> time   = ascent.time;
    // [data-read]
    const rocketplot::LineSeries* series = plot->addLine(time, ascent.velocity, "Velocity");

    const std::size_t       count   = series->size();
    const double            firstX  = series->x(0);
    const double            lastY   = series->y(count - 1);
    const rocketplot::Range xBounds = series->xBounds();  // of the points that aren't gaps
    const rocketplot::Range yBounds = series->yBounds();

    // The point nearest an x value: what the legend reads out at the crosshair.
    if (const std::optional<std::size_t> index = series->nearestIndex(150.0))
    {
        const double velocityAtCutoff = series->y(*index);
        // ...
        // [/data-read]
        static_cast<void>(velocityAtCutoff);
        // [data-read]
    }
    // [/data-read]
    static_cast<void>(firstX);
    static_cast<void>(lastY);
    static_cast<void>(xBounds);
    static_cast<void>(yBounds);
}

void uniformSampling(PlotWidget* plot)
{
    // A vibration: two tones and some noise, sampled at 1 kHz for a quarter of a second.
    const std::size_t   count   = 250;
    std::vector<double> samples = noise(count, 0.05, 11);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double time = static_cast<double>(i) / 1000.0;
        samples[i] += std::sin(2.0 * std::numbers::pi * 12.0 * time) +
                      (0.4 * std::sin(2.0 * std::numbers::pi * 87.0 * time));
    }
    // [data-uniform]
    // 250 samples at 1 kHz: point i is at 0 s + i * 1 ms. No array of x values is stored.
    plot->addLine(rocketplot::UniformX{.start = 0.0, .step = 0.001}, samples);
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (g)");
    // [/data-uniform]
    plot->setTitle("Accelerometer, 1 kHz");
}

void gaps(PlotWidget* plot)
{
    const std::vector<double> time   = linspace(0.0, 60.0, 601);
    std::vector<double>       signal = settling(time, 20.0, 80.0, 15.0, 0.4, 5);
    // [data-gaps]
    // The link dropped out twice: those readings are missing.
    const double missing = std::numeric_limits<double>::quiet_NaN();
    std::fill(signal.begin() + 180, signal.begin() + 230, missing);
    std::fill(signal.begin() + 400, signal.begin() + 420, missing);
    plot->addLine(time, signal);
    // [/data-gaps]
    plot->setTitle("Telemetry with dropouts");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
}

void liveData(PlotWidget* plot)
{
    const std::vector<double> allTimes    = linspace(0.0, 90.0, 901);
    const std::vector<double> allReadings = settling(allTimes, 40.0, 68.0, 40.0, 0.12, 9);
    // [data-live]
    // An empty series to begin with; x shows the newest minute and scrolls as data comes in.
    rocketplot::LineSeries* live =
        plot->addLine(std::vector<double>{}, std::vector<double>{}, "Chamber pressure");
    plot->xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
    plot->xAxis()->setFollowWindow(60.0);
    plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    // [/data-live]
    plot->setTitle("The newest minute");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Pressure (bar)");
    // What a timer or a socket's slot would do as readings arrive: here batch after batch, at once.
    const auto part = [](const std::vector<double>& values, std::size_t first, std::size_t count) {
        const auto begin = values.begin() + static_cast<std::ptrdiff_t>(first);
        return std::vector<double>(begin, begin + static_cast<std::ptrdiff_t>(count));
    };
    const std::size_t batch = 20;
    std::size_t       first = 0;
    for (; first + (2 * batch) <= allTimes.size(); first += batch)
    {
        live->append(part(allTimes, first, batch), part(allReadings, first, batch));
    }
    const std::vector<double> times    = part(allTimes, first, allTimes.size() - first);
    const std::vector<double> readings = part(allReadings, first, allReadings.size() - first);
    // [data-append]
    // In the GUI thread, whenever readings have arrived: all of them in one call.
    live->append(times, readings);

    live->append(90.1, 65.1);  // or a single point
    // [/data-append]
}

}  // namespace

void addDataExamples(Examples& examples)
{
    examples.demonstrations.push_back(
        {.name = QStringLiteral("handing data over"), .run = handingDataOver});
    examples.demonstrations.push_back(
        {.name = QStringLiteral("reading data back"), .run = readingDataBack});
    examples.addPlot(QStringLiteral("data-uniform"), uniformSampling);
    examples.addPlot(QStringLiteral("data-gaps"), gaps);
    examples.addPlot(QStringLiteral("data-live"), liveData);
}

}  // namespace rocketplot::guide
