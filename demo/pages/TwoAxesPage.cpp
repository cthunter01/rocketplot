#include <QCheckBox>
#include <QHBoxLayout>
#include <QString>
#include <QVBoxLayout>
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
    auto* page     = new QWidget(parent);
    auto* layout   = new QVBoxLayout(page);
    auto* controls = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    auto* aligned = new QCheckBox(QStringLiteral("Zeros in line"), page);
    aligned->setToolTip(QStringLiteral("0 at the same height on both y axes"));
    controls->addWidget(aligned);
    controls->addStretch(1);
    layout->addLayout(controls);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle("Ascent");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    plot->yAxis2()->setLabel("Velocity (m/s)");

    const Ascent ascent = simulateAscent(300.0, 10.0, 3);
    plot->addLine(ascent.time, ascent.acceleration, "Acceleration");
    // The right axis appears once a series uses it, and autoscales on its own.
    plot->addLine(ascent.time, ascent.velocity, "Velocity")->setYAxis(plot->yAxis2());
    plot->addHorizontalLine(0.0);

    // Each axis fits its own data: 0 is at one height on the left and another on the right,
    // until the zeros are lined up.
    plot->setYAxesZeroAligned(true);
    // [/snippet]
    aligned->setChecked(plot->areYAxesZeroAligned());
    QObject::connect(aligned, &QCheckBox::toggled, plot,
                     &rocketplot::PlotWidget::setYAxesZeroAligned);
    QObject::connect(plot, &rocketplot::PlotWidget::yAxesZeroAlignedChanged, aligned,
                     [=] { aligned->setChecked(plot->areYAxesZeroAligned()); });
    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage twoAxesPage()
{
    return {
        .title       = QStringLiteral("Two y axes"),
        .description = QStringLiteral(
            "Series can use the secondary y axis on the right, for a second unit. Scroll over "
            "either y axis to zoom just that one; Shift+scroll zooms both. With the zeros in line, "
            "0 is at the same height on both axes while they autoscale (double-click to fit "
            "again)."),
        .sourceFile = QStringLiteral("TwoAxesPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
