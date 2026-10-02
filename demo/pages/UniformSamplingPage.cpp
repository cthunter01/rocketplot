#include <QString>
#include <QWidget>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/UniformX.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Mixed sample rates");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (g)");

    // Ten seconds of a 1 kHz accelerometer: a 7 Hz vibration on a slow ramp, plus noise.
    constexpr double    kRate = 1000.0;
    std::vector<double> raw   = gaussian(10'000, 0.0, 0.08, 7);
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        const double t = static_cast<double>(i) / kRate;
        raw[i] += (0.15 * t) + (0.3 * std::sin(2.0 * std::numbers::pi * 7.0 * t));
    }
    // The same channel averaged down to 50 Hz.
    std::vector<double> averaged;
    for (std::size_t i = 0; i + 20 <= raw.size(); i += 20)
    {
        double sum = 0.0;
        for (std::size_t k = i; k < i + 20; ++k)
        {
            sum += raw[k];
        }
        averaged.push_back(sum / 20.0);
    }

    // No x arrays: x is start + i * step. The averaged samples sit at the middle of their windows.
    plot->addLine(rocketplot::UniformX{.start = 0.0, .step = 1.0 / kRate}, raw,
                  "Accelerometer (1 kHz)");
    plot->addLine(rocketplot::UniformX{.start = 0.0095, .step = 1.0 / 50.0}, averaged,
                  "Averaged (50 Hz)");
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage uniformSamplingPage()
{
    return {
        .title       = QStringLiteral("Uniform sampling"),
        .description = QStringLiteral(
            "Uniformly sampled channels need no x array: <code>UniformX{start, step}</code> gives "
            "x = start + i·step. Channels with different rates share one time axis."),
        .sourceFile = QStringLiteral("UniformSamplingPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
