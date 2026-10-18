// Examples of legend.md.

#include <QAction>
#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QMenu>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

void chillDown(PlotWidget* plot)
{
    const std::vector<double> time = linspace(0.0, 600.0, 1201);
    plot->addLine(time, settling(time, 18.0, -160.0, 120.0, 1.2, 1), "LOX feed line");
    plot->addLine(time, settling(time, 21.0, -95.0, 200.0, 0.9, 2), "Turbopump housing");
    plot->addLine(time, settling(time, 20.0, -40.0, 300.0, 0.7, 3), "Tank wall");
    plot->setTitle("Engine chill-down");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
}

// One run of a chill-down test: two sensors, and how its lines are drawn.
struct Run
{
    std::vector<double> feed;
    std::vector<double> housing;
    Qt::PenStyle        style = Qt::SolidLine;
};

void groups(PlotWidget* plot)
{
    const std::vector<double> time = linspace(0.0, 600.0, 1201);
    const std::vector<Run>    runs{
        {.feed    = settling(time, 18.0, -160.0, 120.0, 0.2, 1),
         .housing = settling(time, 21.0, -95.0, 200.0, 0.2, 3),
         .style   = Qt::SolidLine},
        {.feed    = settling(time, 18.0, -148.0, 120.0, 0.2, 2),
         .housing = settling(time, 21.0, -83.0, 200.0, 0.2, 4),
         .style   = Qt::DashLine},
    };
    plot->setTitle("Engine chill-down, two runs");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
    // [legend-groups]
    // Two runs of the same test: one entry per sensor for both, the runs told apart by line style.
    for (const Run& run : runs)
    {
        rocketplot::LineSeries* feed = plot->addLine(time, run.feed, "LOX feed line");
        feed->setLegendGroup("feed");
        feed->setColor(QColor("#2a78d6"));
        feed->setLineStyle(run.style);
        rocketplot::LineSeries* housing = plot->addLine(time, run.housing, "Turbopump housing");
        housing->setLegendGroup("housing");
        housing->setColor(QColor("#e8743b"));
        housing->setLineStyle(run.style);
    }

    // An annotation can have an entry too, after the series'.
    plot->addHorizontalLine(-150.0)->setLegendName("Feed line target");
    // [/legend-groups]
}

// The pointer on the legend's second entry.
QPointF secondEntry(const PlotWidget& plot)
{
    const QRectF legend = plot.legendArea();
    return {legend.left() + (legend.width() / 2.0), legend.center().y()};
}

void options(const QString& /*scratch*/)
{
    const PlotWidget  window;
    const PlotWidget* plot = &window;
    // [legend-place]
    rocketplot::Legend* legend = plot->legend();
    legend->setAnchor(rocketplot::LegendAnchor::BOTTOM_RIGHT);  // a corner or the middle of an edge
    legend->setAnchor(rocketplot::LegendAnchor::BEST);          // the default: where it hides least

    // Anywhere: fractions of the room the plot area leaves around the legend. (0, 0) is the top
    // left corner, (1, 1) the bottom right. This is what dragging the legend sets.
    legend->setPosition({0.5, 0.0});
    // [/legend-place]

    // [legend-options]
    legend->setVisible(true);   // show it even for a single series
    legend->setVisible(false);  // never show it
    legend->resetVisible();     // the default: shown for two or more named series

    legend->setValuesVisible(false);  // no values at the crosshair
    legend->setInteractive(false);    // no clicking, pointing, dragging or menu
    // [/legend-options]

    // [legend-menu]
    QObject::connect(legend, &rocketplot::Legend::entryMenuAboutToShow, plot,
                     [](QMenu* menu, rocketplot::Series* series) {
                         menu->addSeparator();
                         menu->addAction("Copy name", series, [series] {
                             QGuiApplication::clipboard()->setText(series->name());
                         });
                     });
    // [/legend-menu]
}

}  // namespace

void addLegendExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("legend"), chillDown);
    examples.addPlot(QStringLiteral("legend-highlight"), chillDown, {720, 400}, secondEntry);
    examples.addPlot(QStringLiteral("legend-groups"), groups);
    examples.demonstrations.push_back({.name = QStringLiteral("legend options"), .run = options});
}

}  // namespace rocketplot::guide
