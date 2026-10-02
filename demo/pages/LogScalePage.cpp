#include <QString>
#include <QWidget>
#include <array>
#include <cmath>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Low-pass filter response");
    plot->xAxis()->setLabel("Frequency (Hz)");
    plot->yAxis()->setLabel("Gain");
    plot->xAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    plot->yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    plot->xAxis()->setNumberFormat(rocketplot::NumberFormat::SI);  // 1k, 10k, 100k
    plot->xAxis()->setMinorGridVisible(true);
    plot->yAxis()->setMinorGridVisible(true);

    // A second-order low-pass at 1 kHz, for three damping ratios. Points evenly spaced in log(f).
    constexpr double    kCutoff = 1000.0;
    std::vector<double> frequency;
    for (int i = 0; i <= 600; ++i)
    {
        frequency.push_back(std::pow(10.0, i / 100.0));  // 1 Hz to 1 MHz
    }
    for (const double damping : std::array{0.05, 0.3, 1.0})
    {
        std::vector<double> gain;
        for (const double f : frequency)
        {
            const double r = f / kCutoff;
            gain.push_back(
                1.0 / std::sqrt(std::pow(1.0 - (r * r), 2.0) + std::pow(2.0 * damping * r, 2.0)));
        }
        plot->addLine(frequency, gain, QString("ζ = %1").arg(damping));
    }
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage logScalePage()
{
    return {
        .title       = QStringLiteral("Log scales"),
        .description = QStringLiteral(
            "Logarithmic axes label powers of ten, with 2× and 5× when there is room, and minor "
            "ticks "
            "(and here a minor grid) at the other multiples. The x axis uses SI prefixes. Zooming "
            "is "
            "multiplicative; zoom in far enough and the labels switch to linear steps."),
        .sourceFile = QStringLiteral("LogScalePage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
