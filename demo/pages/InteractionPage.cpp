#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPointF>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <optional>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/LineSeries.h"
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
    auto* back      = new QPushButton(QStringLiteral("Back"), page);
    auto* forward   = new QPushButton(QStringLiteral("Forward"), page);
    auto* reset     = new QPushButton(QStringLiteral("Reset view"), page);
    auto* crosshair = new QCheckBox(QStringLiteral("Crosshair"), page);
    auto* rightDrag = new QCheckBox(QStringLiteral("Right-drag zooms to a box"), page);
    auto* readout   = new QLabel(page);
    readout->setTextFormat(Qt::RichText);
    controls->addWidget(back);
    controls->addWidget(forward);
    controls->addWidget(reset);
    controls->addWidget(crosshair);
    controls->addWidget(rightDrag);
    controls->addStretch(1);
    controls->addWidget(readout);
    layout->addLayout(controls);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Acceleration (m/s²)");
    plot->yAxis2()->setLabel("Altitude (km)");
    const Ascent ascent = simulateAscent(300.0, 50.0, 7);
    plot->addLine(ascent.time, ascent.acceleration, "Acceleration");
    plot->addLine(ascent.time, ascent.altitude, "Altitude")->setYAxis(plot->yAxis2());
    plot->setCrosshairEnabled(true);

    // The view history: back and forward through the user's pans and zooms.
    QObject::connect(back, &QPushButton::clicked, plot, &rocketplot::PlotWidget::back);
    QObject::connect(forward, &QPushButton::clicked, plot, &rocketplot::PlotWidget::forward);
    QObject::connect(reset, &QPushButton::clicked, plot, &rocketplot::PlotWidget::resetView);
    const auto updateButtons = [=] {
        back->setEnabled(plot->canGoBack());
        forward->setEnabled(plot->canGoForward());
    };
    QObject::connect(plot, &rocketplot::PlotWidget::historyChanged, page, updateButtons);

    // Where the crosshair is, in data coordinates.
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, readout, [=] {
        const std::optional<QPointF> position = plot->crosshairPosition();
        readout->setText(position ? QStringLiteral("t = %1 s, a = %2 m/s²")
                                        .arg(position->x(), 0, 'f', 2)
                                        .arg(position->y(), 0, 'f', 2)
                                  : QString());
    });

    // Gestures can be rebound: here the right button can also zoom to a box.
    QObject::connect(rightDrag, &QCheckBox::toggled, plot, [plot](bool on) {
        rocketplot::InputBindings bindings = rocketplot::InputBindings::defaults();
        if (on)
        {
            bindings.bind(rocketplot::Gesture::DRAG, Qt::RightButton, Qt::NoModifier,
                          rocketplot::PlotAction::BOX_ZOOM);
        }
        plot->setInputBindings(bindings);
    });
    // [/snippet]
    crosshair->setChecked(plot->isCrosshairEnabled());
    QObject::connect(crosshair, &QCheckBox::toggled, plot,
                     &rocketplot::PlotWidget::setCrosshairEnabled);
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairEnabledChanged, crosshair,
                     [=] { crosshair->setChecked(plot->isCrosshairEnabled()); });
    updateButtons();
    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage interactionPage()
{
    return {
        .title       = QStringLiteral("Interaction"),
        .description = QStringLiteral(
            "Drag to pan, <b>Shift-drag</b> to zoom to a box (a thin box zooms just one axis), "
            "scroll to zoom, double-click to fit the data again. On a trackpad, scroll to pan and "
            "pinch to zoom; on a touchscreen, drag and pinch. Over an axis, each of these only "
            "affects that axis. The mouse's back and forward buttons step through the view "
            "history, and right-clicking opens a menu with the history and the crosshair."),
        .sourceFile = QStringLiteral("InteractionPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
