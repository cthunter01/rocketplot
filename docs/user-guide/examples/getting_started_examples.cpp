// Examples of getting-started.md.

#include <QString>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

// What first_plot.cpp shows in its window.
void firstPlot(PlotWidget* plot)
{
    std::vector<double> time;
    std::vector<double> altitude;
    for (int second = 0; second <= 300; ++second)
    {
        time.push_back(second);
        altitude.push_back(0.0006 * second * second);
    }
    plot->setTitle("Ascent");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Altitude (km)");
    plot->addLine(time, altitude);
}

void partsOfAPlot(PlotWidget* plot)
{
    const Ascent              ascent  = sampleAscent();
    const std::vector<double> time    = ascent.time;
    const std::vector<double> nominal = ascent.altitude;
    std::vector<double>       lofted  = ascent.altitude;
    for (double& altitude : lofted)
    {
        altitude *= 1.25;
    }
    // [plot-parts]
    // The plot makes each series, owns it, and hands back a pointer to set it up with.
    rocketplot::LineSeries* line = plot->addLine(time, nominal, "Nominal");
    line->setLineWidth(3.0);
    plot->addLine(time, lofted, "Lofted")->setLineStyle(Qt::DashLine);

    // The axes and the legend are always there.
    plot->setTitle("Ascent profiles");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Altitude (km)");
    plot->legend()->setAnchor(rocketplot::LegendAnchor::TOP_LEFT);
    // [/plot-parts]
}

}  // namespace

void addGettingStartedExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("first-plot"), firstPlot);
    examples.addPlot(QStringLiteral("plot-parts"), partsOfAPlot);
}

}  // namespace rocketplot::guide
