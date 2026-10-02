#include <QString>
#include <QWidget>
#include <algorithm>
#include <limits>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Telemetry dropouts");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Pressure (bar)");

    // Two minutes at 100 Hz.
    const std::vector<double> time = linspace(0.0, 120.0, 12001);
    std::vector<double>       a    = warmUp(time, 2.0, 18.0, 30.0, 0.15, 4);
    std::vector<double>       b    = warmUp(time, 1.0, 16.0, 45.0, 0.15, 5);

    // NaN marks a missing sample: the line breaks there.
    addDropouts(b, 8, 600, 6);
    // So does infinity, and neither one affects autoscale.
    a[3000] = std::numeric_limits<double>::infinity();
    // A lone sample between two gaps is drawn as a dot.
    std::fill(a.begin() + 7000, a.begin() + 7400, std::numeric_limits<double>::quiet_NaN());
    a[7200] = 10.0;

    plot->addLine(time, a, "Sensor A");
    plot->addLine(time, b, "Sensor B (with dropouts)");
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage gapsPage()
{
    return {.title       = QStringLiteral("Gaps (NaN)"),
            .description = QStringLiteral("NaN and infinite values are gaps: the line breaks "
                                          "there, and autoscale ignores them. Zoom "
                                          "in on a dropout: gaps stay exact at every zoom level, "
                                          "even when millions of points share "
                                          "a pixel."),
            .sourceFile  = QStringLiteral("GapsPage.cpp"),
            .create      = create};
}

}  // namespace rocketplot::demo
