#include <QString>
#include <QWidget>
#include <Qt>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Landing dispersion");
    plot->xAxis()->setLabel("Crossrange (m)");
    plot->yAxis()->setLabel("Downrange (m)");

    // Three Monte Carlo runs: 400 landing points each, a different marker per run.
    struct Run
    {
        QString            name;
        double             x;
        double             y;
        double             sigma;
        rocketplot::Marker marker;
    };
    const std::array<Run, 3> runs{{{.name   = "Nominal winds",
                                    .x      = 0.0,
                                    .y      = 0.0,
                                    .sigma  = 18.0,
                                    .marker = rocketplot::Marker::CIRCLE},
                                   {.name   = "Gusty",
                                    .x      = 25.0,
                                    .y      = 40.0,
                                    .sigma  = 30.0,
                                    .marker = rocketplot::Marker::SQUARE},
                                   {.name   = "Engine-out",
                                    .x      = -60.0,
                                    .y      = -35.0,
                                    .sigma  = 22.0,
                                    .marker = rocketplot::Marker::TRIANGLE}}};
    std::uint64_t            seed = 1;
    for (const Run& run : runs)
    {
        auto* points = plot->addScatter(gaussian(400, run.x, run.sigma, seed),
                                        gaussian(400, run.y, run.sigma, seed + 100), run.name);
        points->setMarker(run.marker);
        ++seed;
    }

    // A line needn't have sorted x: this one is a circle.
    std::vector<double> x;
    std::vector<double> y;
    for (int degree = 0; degree <= 360; ++degree)
    {
        const double angle = degree * std::numbers::pi / 180.0;
        x.push_back(100.0 * std::cos(angle));
        y.push_back(100.0 * std::sin(angle));
    }
    auto* limit = plot->addLine(x, y, "100 m keep-out");
    limit->setLineStyle(Qt::DashLine);
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage scatterPage()
{
    return {
        .title       = QStringLiteral("Scatter & markers"),
        .description = QStringLiteral(
            "Scatter series draw a marker per point, each inside a ring of the background color so "
            "overlapping markers stay apart. Lines can be unsorted in x: the dashed keep-out ring "
            "is a line traced around a circle."),
        .sourceFile = QStringLiteral("ScatterPage.cpp"),
        .create     = create};
}

}  // namespace rocketplot::demo
