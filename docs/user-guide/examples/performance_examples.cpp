// Examples of performance.md.

#include <QLoggingCategory>
#include <QString>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/UniformX.h"

namespace rocketplot::guide
{

namespace
{

// Two million samples of a noisy vibration, with the debug overlay on.
void overlay(PlotWidget* plot)
{
    const std::size_t   count   = 2'000'000;
    std::vector<double> samples = noise(count, 0.3, 21);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double time = static_cast<double>(i) / 10'000.0;
        samples[i] += (1.0 + (0.5 * std::sin(2.0 * std::numbers::pi * time / 80.0))) *
                      std::sin(2.0 * std::numbers::pi * time / 7.0);
    }
    plot->setTitle("Vibration, 10 kHz");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (g)");
    // [performance-overlay]
    plot->addLine(rocketplot::UniformX{.start = 0.0, .step = 1.0 / 10'000.0}, std::move(samples),
                  "Accelerometer");
    plot->setDebugOverlay(true);  // or run with ROCKETPLOT_DEBUG_OVERLAY=1 in the environment
    // [/performance-overlay]
}

void measuring(const QString& /*scratch*/)
{
    PlotWidget                window;
    PlotWidget*               plot = &window;
    const std::vector<double> time = linspace(0.0, 10.0, 1001);
    // [performance-sorted]
    const rocketplot::LineSeries* series = plot->addLine(time, time, "Ramp");
    if (!series->isSortedByX())
    {
        // x goes back somewhere (or has a NaN): every point is looked at for every frame.
    }
    // [/performance-sorted]

    // [performance-logging]
    // What the library does, on Qt's debug output: frame times, pans and zooms, data arriving.
    QLoggingCategory::setFilterRules(
        "rocketplot.render.debug=true\n"
        "rocketplot.input.debug=true\n"
        "rocketplot.data.debug=true");
    // [/performance-logging]
    QLoggingCategory::setFilterRules(QString());
}

}  // namespace

void addPerformanceExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("performance-overlay"), overlay);
    examples.demonstrations.push_back({.name = QStringLiteral("measuring"), .run = measuring});
}

}  // namespace rocketplot::guide
