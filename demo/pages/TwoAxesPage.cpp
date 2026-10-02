#include <QString>
#include <QWidget>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Ascent");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Altitude (km)");
    plot->yAxis2()->setLabel("Velocity (m/s)");

    const Ascent ascent = simulateAscent(300.0, 10.0, 3);
    plot->addLine(ascent.time, ascent.altitude, "Altitude");
    // The right axis appears once a series uses it, and autoscales on its own.
    plot->addLine(ascent.time, ascent.velocity, "Velocity")->setYAxis(plot->yAxis2());
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage twoAxesPage()
{
    return {
        .title       = QStringLiteral("Two y axes"),
        .description = QStringLiteral("Series can use the secondary y axis on the right, for a "
                                      "second unit. Scroll over either y "
                                      "axis to zoom just that one; Shift+scroll zooms both."),
        .sourceFile  = QStringLiteral("TwoAxesPage.cpp"),
        .create      = create,
    };
}

}  // namespace rocketplot::demo
