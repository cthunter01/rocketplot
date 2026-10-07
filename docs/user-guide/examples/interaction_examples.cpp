// Examples of interaction.md.

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QObject>
#include <QPointF>
#include <QPushButton>
#include <QString>
#include <Qt>
#include <cstddef>
#include <optional>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

// Two trajectories to point at.
void addTrajectories(PlotWidget* plot)
{
    const Ascent              ascent = sampleAscent();
    const std::vector<double> time   = ascent.time;
    std::vector<double>       lofted = ascent.altitude;
    for (double& altitude : lofted)
    {
        altitude *= 1.25;
    }
    plot->addLine(time, ascent.altitude, "Nominal");
    plot->addLine(time, lofted, "Lofted");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Altitude (km)");
    plot->legend()->setAnchor(LegendAnchor::TOP_LEFT);
}

void crosshair(PlotWidget* plot)
{
    addTrajectories(plot);
    // [interaction-crosshair]
    plot->setCrosshairEnabled(true);  // off by default; the context menu has a switch for it
    // [/interaction-crosshair]
}

void tracing(PlotWidget* plot)
{
    addTrajectories(plot);
    // [interaction-crosshair-mode]
    plot->setCrosshairEnabled(true);
    plot->setCrosshairMode(rocketplot::CrosshairMode::TRACE);  // FREE to begin with
    // [/interaction-crosshair-mode]
}

void following(const QString& /*scratch*/)
{
    PlotWidget   window;
    PlotWidget*  plot = &window;
    QLabel       label;
    QLabel*      status = &label;
    QPushButton  button;
    QPushButton* backButton = &button;
    // [interaction-crosshair-signal]
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, status, [plot, status] {
        if (const std::optional<QPointF> at = plot->crosshairPosition())
        {
            status->setText(QString("t = %1 s").arg(at->x(), 0, 'f', 1));
        }
        else
        {
            status->clear();  // the pointer left the plot
        }
    });
    // [/interaction-crosshair-signal]

    // [interaction-crosshair-point]
    QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, status, [plot, status] {
        if (const rocketplot::Series* series = plot->crosshairSeries())
        {
            // On a data point: of this series, and this one of its points.
            const std::size_t index = plot->crosshairIndex().value_or(0);
            status->setText(QString("%1: %2 km at t = %3 s")
                                .arg(series->name())
                                .arg(series->y(index), 0, 'f', 2)
                                .arg(series->x(index), 0, 'f', 1));
        }
    });
    // [/interaction-crosshair-point]

    // [interaction-history]
    plot->back();     // the view before the user's last pan, zoom or reset
    plot->forward();  // and forward again

    // A toolbar button that is only enabled when there is something to go back to.
    QObject::connect(plot, &rocketplot::PlotWidget::historyChanged, backButton,
                     [plot, backButton] { backButton->setEnabled(plot->canGoBack()); });
    QObject::connect(backButton, &QPushButton::clicked, plot, &rocketplot::PlotWidget::back);
    // [/interaction-history]

    // [interaction-view-signal]
    QObject::connect(plot, &rocketplot::PlotWidget::viewChanged, status, [plot, status] {
        // The range of an axis changed: by the user, by autoscale or by code.
        const rocketplot::Range shown = plot->xAxis()->range();
        status->setText(QString("Showing %1 s").arg(shown.span(), 0, 'f', 1));
    });
    // [/interaction-view-signal]

    // [interaction-menu]
    QObject::connect(plot, &rocketplot::PlotWidget::contextMenuAboutToShow, plot,
                     [plot](QMenu* menu, QPointF position) {
                         // Where the menu was asked for, in data coordinates.
                         const double x = plot->mapToData(position).x();
                         menu->addSeparator();
                         menu->addAction(QString("Mark t = %1 s").arg(x, 0, 'f', 1), plot,
                                         [plot, x] { plot->addEvent(x, "Mark"); });
                     });
    // [/interaction-menu]

    // [interaction-bindings]
    rocketplot::InputBindings bindings = rocketplot::InputBindings::defaults();
    // Zoom to a box with the right button too, and let the wheel zoom the time axis only.
    bindings.bind(rocketplot::Gesture::DRAG, Qt::RightButton, Qt::NoModifier,
                  rocketplot::PlotAction::BOX_ZOOM);
    bindings.bind(rocketplot::Gesture::WHEEL, Qt::NoModifier, rocketplot::PlotAction::ZOOM_X);
    // A double click does nothing.
    bindings.bind(rocketplot::Gesture::DOUBLE_CLICK, Qt::LeftButton, Qt::NoModifier,
                  rocketplot::PlotAction::NONE);
    plot->setInputBindings(bindings);
    // [/interaction-bindings]

    // [interaction-wheel-off]
    rocketplot::InputBindings scrolling = plot->inputBindings();
    scrolling.bind(rocketplot::Gesture::WHEEL, Qt::NoModifier, rocketplot::PlotAction::NONE);
    plot->setInputBindings(scrolling);  // Ctrl + wheel and Shift + wheel still zoom
    // [/interaction-wheel-off]

    // [interaction-static]
    plot->setInputBindings(rocketplot::InputBindings());  // no gesture does anything
    plot->setContextMenuPolicy(Qt::NoContextMenu);        // no menu
    plot->legend()->setInteractive(false);                // the legend doesn't react either
    // [/interaction-static]
}

}  // namespace

void addInteractionExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("interaction-crosshair"), crosshair, {720, 400},
                     [](const PlotWidget& plot) {
                         const QRectF area = plot.plotArea();
                         return QPointF(area.left() + (0.57 * area.width()),
                                        area.top() + (0.45 * area.height()));
                     });
    // The pointer is in the same place as in the figure before: above both lines.
    examples.addPlot(QStringLiteral("interaction-crosshair-trace"), tracing, {720, 400},
                     [](const PlotWidget& plot) {
                         const QRectF area = plot.plotArea();
                         return QPointF(area.left() + (0.57 * area.width()),
                                        area.top() + (0.45 * area.height()));
                     });
    examples.demonstrations.push_back(
        {.name = QStringLiteral("following the user"), .run = following});
}

}  // namespace rocketplot::guide
