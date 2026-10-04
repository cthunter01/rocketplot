// Examples of multiple-plots.md.

#include <QDir>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

QWidget* linkedPlots(QWidget* parent)
{
    const Ascent ascent = sampleAscent();
    auto*        window = new QWidget(parent);
    auto*        layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // [multiple-link]
    // Three plots of their own, one above the other in a layout.
    auto* altitude     = new rocketplot::PlotWidget(window);
    auto* velocity     = new rocketplot::PlotWidget(window);
    auto* acceleration = new rocketplot::PlotWidget(window);
    altitude->addLine(ascent.time, ascent.altitude);
    velocity->addLine(ascent.time, ascent.velocity);
    acceleration->addLine(ascent.time, ascent.acceleration);
    altitude->yAxis()->setLabel("Altitude (km)");
    velocity->yAxis()->setLabel("Velocity (m/s)");
    acceleration->yAxis()->setLabel("Accel. (m/s²)");
    acceleration->xAxis()->setLabel("Time (s)");

    // The link ties their x axes together. It belongs to the window, like the plots.
    auto* link = new rocketplot::PlotLink(window);
    for (rocketplot::PlotWidget* plot : {altitude, velocity, acceleration})
    {
        plot->setCrosshairEnabled(true);
        link->addPlot(plot);
        layout->addWidget(plot);
    }
    // [/multiple-link]
    for (PlotWidget* plot : {altitude, velocity, acceleration})
    {
        plot->setThemeMode(ThemeMode::LIGHT);
    }
    return window;
}

QWidget* gridOfRows(QWidget* parent)
{
    const Ascent              ascent = sampleAscent();
    const std::vector<double> time   = ascent.time;
    // [multiple-grid]
    auto* grid = new rocketplot::PlotGrid(3, 1, parent);  // three rows, one column
    grid->setTitle("Ascent");
    grid->plot(0)->addLine(time, ascent.altitude);
    grid->plot(1)->addLine(time, ascent.velocity);
    grid->plot(2)->addLine(time, ascent.acceleration);
    grid->plot(0)->yAxis()->setLabel("Altitude (km)");
    grid->plot(1)->yAxis()->setLabel("Velocity (m/s)");
    grid->plot(2)->yAxis()->setLabel("Accel. (m/s²)");
    grid->plot(2)->xAxis()->setLabel("Time (s)");
    grid->setCrosshairEnabled(true);  // for every plot
    // [/multiple-grid]
    grid->setThemeMode(ThemeMode::LIGHT);
    return grid;
}

QWidget* gridOfColumns(QWidget* parent)
{
    const Ascent ascent = sampleAscent();
    const auto   index  = [&ascent](double time) {
        return static_cast<std::size_t>(std::ranges::lower_bound(ascent.time, time) -
                                        ascent.time.begin());
    };
    const std::size_t cutoff   = index(150.0);
    const std::size_t ignition = index(160.0);
    const std::span   time(ascent.time);
    const std::span   altitude(ascent.altitude);
    const std::span   velocity(ascent.velocity);
    // [multiple-grid-columns]
    auto* grid = new rocketplot::PlotGrid(2, 2, parent);
    grid->plot(0, 0)->setTitle("First stage");
    grid->plot(0, 1)->setTitle("Second stage");
    // The left column: the first stage's burn. Its two plots share their time axis.
    grid->plot(0, 0)->addLine(time.first(cutoff), altitude.first(cutoff));
    grid->plot(1, 0)->addLine(time.first(cutoff), velocity.first(cutoff));
    // The right column: the second stage's, on a time axis of its own.
    grid->plot(0, 1)->addLine(time.subspan(ignition), altitude.subspan(ignition));
    grid->plot(1, 1)->addLine(time.subspan(ignition), velocity.subspan(ignition));

    grid->plot(0, 0)->yAxis()->setLabel("Altitude (km)");
    grid->plot(1, 0)->yAxis()->setLabel("Velocity (m/s)");
    grid->plot(1, 0)->xAxis()->setLabel("Time (s)");
    grid->plot(1, 1)->xAxis()->setLabel("Time (s)");
    // [/multiple-grid-columns]
    grid->setThemeMode(ThemeMode::LIGHT);
    return grid;
}

void options(const QString& scratch)
{
    PlotWidget              first;
    PlotWidget              second;
    rocketplot::PlotLink    owner;
    rocketplot::PlotLink*   link = &owner;
    rocketplot::PlotWidget* plot = &second;
    link->addPlot(&first);
    link->addPlot(&second);
    // [multiple-link-options]
    link->setAlignMargins(false);   // each plot keeps the margins its own labels need
    link->setLinkCrosshair(false);  // a plot's crosshair doesn't show in the others
    link->removePlot(plot);         // the plot goes its own way again
    // [/multiple-link-options]

    rocketplot::PlotGrid  widget(3, 2);
    rocketplot::PlotGrid* grid = &widget;
    const QDir            folder(scratch);
    // [multiple-grid-options]
    grid->setXLink(rocketplot::GridLink::ALL);      // every plot on one x axis
    grid->setXLink(rocketplot::GridLink::NONE);     // or each on its own
    grid->setXLink(rocketplot::GridLink::COLUMNS);  // the default: column by column

    grid->setInnerTickLabelsVisible(true);  // the plots above the bottom row label their x axis too
    grid->setRowStretch(0, 2);              // the top row twice as tall as the others
    grid->setSpacing(8);                    // pixels between neighboring plots
    grid->setGridSize(4, 2);                // a fourth row: the plots that exist stay as they are

    grid->setThemeMode(rocketplot::ThemeMode::DARK);  // of every plot, and of plots added later
    grid->resetView();                                // every axis of every plot back to autoscale
    // [/multiple-grid-options]

    // [multiple-grid-output]
    // The whole grid as one page, and as one image.
    const bool pdfWritten = grid->exportTo(folder.filePath("flight.pdf"));
    const bool pngWritten =
        grid->exportImage(folder.filePath("flight.png"),
                          {.size = {900, 700}, .dpi = 200.0, .theme = rocketplot::Theme::print()});

    const QJsonObject state = grid->saveState();  // the state of every plot
    grid->restoreState(state);
    // [/multiple-grid-output]
    static_cast<void>(pdfWritten);
    static_cast<void>(pngWritten);
}

// The pointer in the first plot's area.
QPointF inThePlot(const PlotWidget& plot)
{
    const QRectF area = plot.plotArea();
    return {area.left() + (0.58 * area.width()), area.top() + (0.5 * area.height())};
}

}  // namespace

void addMultiplePlotExamples(Examples& examples)
{
    examples.figures.push_back({
        .name    = QStringLiteral("multiple-link"),
        .make    = linkedPlots,
        .size    = {720, 520},
        .pointer = inThePlot,
    });
    examples.figures.push_back(
        {.name = QStringLiteral("multiple-grid"), .make = gridOfRows, .size = {720, 540}});
    examples.figures.push_back({
        .name = QStringLiteral("multiple-grid-columns"),
        .make = gridOfColumns,
        .size = {720, 460},
    });
    examples.demonstrations.push_back(
        {.name = QStringLiteral("link and grid options"), .run = options});
}

}  // namespace rocketplot::guide
