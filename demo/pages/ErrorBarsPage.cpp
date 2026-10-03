#include <QString>
#include <QWidget>
#include <cstddef>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Thrust calibration");
    plot->xAxis()->setLabel("Chamber pressure (bar)");
    plot->yAxis()->setLabel("Thrust (kN)");

    // A fitted model and its 95% confidence interval. y errors on a line are a band.
    const std::vector<double> pressure = linspace(15.0, 105.0, 200);
    std::vector<double>       model;
    std::vector<double>       confidence;
    for (const double p : pressure)
    {
        model.push_back((8.2 * p) - 10.0);
        confidence.push_back(14.0 + (0.02 * (p - 60.0) * (p - 60.0)));
    }
    auto* fit = plot->addLine(pressure, model, "Model, 95% interval");
    fit->setYErrors(confidence);

    // Test firings, uncertain in both pressure and thrust. On a scatter series, errors are bars;
    // they can differ below and above (here: the load cell reads low when hot).
    const std::vector<double> firingPressure = linspace(20.0, 100.0, 12);
    const std::vector<double> scatter        = gaussian(firingPressure.size(), 0.0, 14.0, 7);
    std::vector<double>       thrust;
    std::vector<double>       below;
    std::vector<double>       above;
    for (std::size_t i = 0; i < firingPressure.size(); ++i)
    {
        thrust.push_back((8.2 * firingPressure[i]) - 10.0 + scatter[i]);
        below.push_back(12.0);
        above.push_back(12.0 + (0.3 * firingPressure[i]));
    }
    auto* firings = plot->addScatter(firingPressure, thrust, "Test firings");
    firings->setYErrors(below, above);
    firings->setXErrors(std::vector<double>(firingPressure.size(), 1.5));
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage errorBarsPage()
{
    return {
        .title       = QStringLiteral("Error bars & bands"),
        .description = QStringLiteral(
            "Any series can carry errors along x and y, the same or different below and above. "
            "Lines show y errors as a band and scatter series as bars (Series::setErrorStyle() "
            "swaps them); autoscale makes room for both. Bars too close together to tell apart "
            "turn into a band, so millions of points with errors stay readable and fast."),
        .sourceFile = QStringLiteral("ErrorBarsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
