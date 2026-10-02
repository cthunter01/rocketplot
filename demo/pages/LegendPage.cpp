#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaEnum>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

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
    auto*           anchor  = new QComboBox(page);
    const QMetaEnum anchors = QMetaEnum::fromType<rocketplot::LegendAnchor>();
    for (int i = 0; i < anchors.keyCount(); ++i)
    {
        anchor->addItem(QString::fromLatin1(anchors.key(i)), anchors.value(i));
    }
    auto* values      = new QCheckBox(QStringLiteral("Values at the crosshair"), page);
    auto* interactive = new QCheckBox(QStringLiteral("Interactive"), page);
    controls->addWidget(new QLabel(QStringLiteral("Anchor:"), page));
    controls->addWidget(anchor);
    controls->addWidget(values);
    controls->addWidget(interactive);
    controls->addStretch(1);
    layout->addLayout(controls);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
    const std::vector<double> time = linspace(0.0, 600.0, 6001);
    plot->addLine(time, warmUp(time, 21.0, 64.0, 180.0, 0.4, 1), "Turbopump housing");
    plot->addLine(time, warmUp(time, 21.0, 38.0, 240.0, 0.3, 2), "Avionics bay");
    plot->addLine(time, warmUp(time, 21.0, 12.0, 400.0, 0.2, 3), "Payload fairing");
    plot->addLine(time, warmUp(time, 18.0, -9.0, 120.0, 0.3, 4), "LOX feed line");
    plot->addLine(time, warmUp(time, 21.0, 4.0, 60.0, 0.2, 5), "Battery");

    // The legend goes where it hides the least data (LegendAnchor::BEST, the default). With the
    // crosshair on, each entry shows its series' value at the pointer.
    plot->setCrosshairEnabled(true);
    rocketplot::Legend* legend = plot->legend();
    // [/snippet]

    anchor->setCurrentIndex(anchor->findData(static_cast<int>(legend->anchor())));
    values->setChecked(legend->areValuesVisible());
    interactive->setChecked(legend->isInteractive());
    QObject::connect(anchor, &QComboBox::currentIndexChanged, legend, [=] {
        legend->setAnchor(static_cast<rocketplot::LegendAnchor>(anchor->currentData().toInt()));
    });
    // Dragging the legend makes it CUSTOM: show that.
    QObject::connect(legend, &rocketplot::Legend::changed, anchor, [=] {
        anchor->setCurrentIndex(anchor->findData(static_cast<int>(legend->anchor())));
    });
    QObject::connect(values, &QCheckBox::toggled, legend, &rocketplot::Legend::setValuesVisible);
    QObject::connect(interactive, &QCheckBox::toggled, legend, &rocketplot::Legend::setInteractive);
    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage legendPage()
{
    return {
        .title       = QStringLiteral("Legend"),
        .description = QStringLiteral(
            "Click an entry to hide or show its series (autoscale fits what is shown), "
            "double-click "
            "one to show it alone, point at one to bring its series forward, and right-click one "
            "to "
            "change its color, line width or marker. Drag the legend anywhere; by default it moves "
            "to where it hides the least data. Move the pointer over the plot to read every series "
            "at that time."),
        .sourceFile = QStringLiteral("LegendPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
