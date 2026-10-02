#include <QString>
#include <QWidget>
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
    plot->setTitle("Engine bay temperatures");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");

    // Ten minutes at 10 Hz. addLine() copies any sized range of numbers.
    const std::vector<double> time = linspace(0.0, 600.0, 6001);
    plot->addLine(time, warmUp(time, 21.0, 64.0, 180.0, 0.4, 1), "Turbopump housing");
    plot->addLine(time, warmUp(time, 21.0, 38.0, 240.0, 0.3, 2), "Avionics bay");
    plot->addLine(time, warmUp(time, 21.0, 12.0, 400.0, 0.2, 3), "Payload fairing");
    // [/snippet]
    return plot;
}

}  // namespace

DemoPage basicLinesPage()
{
    return {
        .title       = QStringLiteral("Basic lines"),
        .description = QStringLiteral(
            "Several series on shared axes, with a legend once there are two or more named series. "
            "Drag to pan, scroll to zoom about the pointer (over an axis: only that axis; Ctrl: x "
            "only, "
            "Shift: y only), double-click to fit the data again."),
        .sourceFile = QStringLiteral("BasicLinesPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
