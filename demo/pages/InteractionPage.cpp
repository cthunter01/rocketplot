#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPointF>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <cstddef>
#include <optional>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
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
    auto* follows   = new QComboBox(page);
    follows->addItem(QStringLiteral("Free"), QVariant::fromValue(rocketplot::CrosshairMode::FREE));
    follows->addItem(QStringLiteral("Snap to data"),
                     QVariant::fromValue(rocketplot::CrosshairMode::SNAP));
    follows->addItem(QStringLiteral("Trace data"),
                     QVariant::fromValue(rocketplot::CrosshairMode::TRACE));
    follows->setToolTip(
        QStringLiteral("What the crosshair follows: the pointer, a data point "
                       "close to the pointer, or always the nearest series"));
    auto* rightDrag = new QCheckBox(QStringLiteral("Right-drag zooms to a box"), page);
    auto* readout   = new QLabel(page);
    readout->setTextFormat(Qt::RichText);
    controls->addWidget(back);
    controls->addWidget(forward);
    controls->addWidget(reset);
    controls->addWidget(crosshair);
    controls->addWidget(follows);
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

    // The crosshair follows the pointer, or the data: a point close to the pointer (SNAP), or
    // always the point of the nearest series at the pointer's x (TRACE).
    QObject::connect(follows, &QComboBox::currentIndexChanged, plot, [=] {
        plot->setCrosshairMode(follows->currentData().value<rocketplot::CrosshairMode>());
    });

    // Where the crosshair is: on a point of a series, or else in data coordinates.
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, readout, [=] {
        const std::optional<QPointF> position = plot->crosshairPosition();
        if (const rocketplot::Series* series = plot->crosshairSeries())
        {
            const std::size_t index = plot->crosshairIndex().value_or(0);
            readout->setText(QStringLiteral("<b>%1</b>: %2 at t = %3 s")
                                 .arg(series->name())
                                 .arg(series->y(index), 0, 'f', 2)
                                 .arg(series->x(index), 0, 'f', 2));
            return;
        }
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
    // The context menu and the inspector change what the crosshair follows too.
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairModeChanged, follows, [=] {
        follows->setCurrentIndex(follows->findData(QVariant::fromValue(plot->crosshairMode())));
    });
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
            "history, and right-clicking opens a menu with the history and the crosshair. The "
            "crosshair follows the pointer, <b>snaps</b> to a data point close to it, or "
            "<b>traces</b> the nearest series: choose which above, or in the menu."),
        .sourceFile = QStringLiteral("InteractionPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
