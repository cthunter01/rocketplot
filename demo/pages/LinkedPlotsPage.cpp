#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // [snippet]
    const Ascent ascent = simulateAscent(300.0, 50.0, 5);
    auto*        link   = new rocketplot::PlotLink(page);
    const auto   add    = [&](const std::vector<double>& values, const QString& label) {
        auto* plot = new rocketplot::PlotWidget(page);
        plot->yAxis()->setLabel(label);
        // Zoom into a stretch of time and each plot fits its y axis to what is visible.
        plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
        plot->setCrosshairEnabled(true);
        plot->addLine(ascent.time, values);
        link->addPlot(plot);  // x pans, zooms and crosshair together; plot areas line up
        layout->addWidget(plot);
        return plot;
    };
    add(ascent.altitude, "Altitude (km)");
    add(ascent.velocity, "Velocity (m/s)");
    add(ascent.acceleration, "Acceleration (m/s²)")->xAxis()->setLabel("Time (s)");
    // [/snippet]
    return page;
}

}  // namespace

DemoPage linkedPlotsPage()
{
    return {
        .title       = QStringLiteral("Linked plots"),
        .description = QStringLiteral(
            "A PlotLink ties the x axes of stacked plots together: pan or zoom in one and the "
            "others follow, the crosshair shows the same time in all of them, and their plot "
            "areas stay aligned however wide each one's labels are. With FIT_VISIBLE, each y axis "
            "fits the data in the current time window (Ctrl+scroll zooms x only). The plots share "
            "one view history: Back undoes a zoom in whichever plot it happened."),
        .sourceFile = QStringLiteral("LinkedPlotsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
