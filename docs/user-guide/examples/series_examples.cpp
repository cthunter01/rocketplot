// Examples of series.md.

#include <QColor>
#include <QPen>
#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

void lines(PlotWidget* plot)
{
    const std::vector<double> time      = linspace(0.0, 10.0, 21);
    std::vector<double>       measured  = noise(time.size(), 0.15, 2);
    std::vector<double>       predicted = time;
    std::vector<double>       limit     = time;
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        predicted[i] = 4.0 * (1.0 - std::exp(-time[i] / 3.0));
        measured[i] += predicted[i];
        limit[i] = 4.5;
    }
    // [series-lines]
    rocketplot::LineSeries* model = plot->addLine(time, predicted, "Predicted");
    model->setLineWidth(3.0);  // device-independent pixels

    rocketplot::LineSeries* samples = plot->addLine(time, measured, "Measured");
    samples->setMarker(rocketplot::Marker::CIRCLE);  // a marker at each point
    samples->setLineWidth(1.0);

    rocketplot::LineSeries* ceiling = plot->addLine(time, limit, "Limit");
    ceiling->setPen(QPen(QColor("#c0392b"), 1.5, Qt::DashLine));  // color, width and style at once
    // [/series-lines]
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Load (g)");
    plot->legend()->setAnchor(LegendAnchor::BOTTOM_RIGHT);
}

void markers(PlotWidget* plot)
{
    const std::vector<double> x = linspace(1.0, 9.0, 9);
    // [series-markers]
    const std::vector<rocketplot::Marker> shapes{
        rocketplot::Marker::CIRCLE,   rocketplot::Marker::SQUARE, rocketplot::Marker::DIAMOND,
        rocketplot::Marker::TRIANGLE, rocketplot::Marker::CROSS,  rocketplot::Marker::PLUS,
    };
    const std::vector<QString> names{"CIRCLE", "SQUARE", "DIAMOND", "TRIANGLE", "CROSS", "PLUS"};
    for (std::size_t i = 0; i < shapes.size(); ++i)
    {
        const std::vector<double>  y(x.size(), static_cast<double>(shapes.size() - i));
        rocketplot::ScatterSeries* points = plot->addScatter(x, y, names[i]);
        points->setMarker(shapes[i]);
        points->setMarkerSize(11.0);  // the diameter, in device-independent pixels
    }
    // [/series-markers]
    plot->xAxis()->setRange(0.0, 13.5);
    plot->yAxis()->setRange(0.0, 7.0);
    plot->legend()->setAnchor(LegendAnchor::RIGHT);
}

void bubbles(PlotWidget* plot)
{
    // Test firings: chamber pressure against thrust, with the burn time and the mixture ratio.
    const std::size_t         count    = 40;
    const std::vector<double> pressure = linspace(40.0, 110.0, count);
    std::vector<double>       thrust   = noise(count, 22.0, 4);
    std::vector<double>       burnTime = noise(count, 9.0, 6);
    std::vector<double>       mixture  = noise(count, 0.25, 8);
    for (std::size_t i = 0; i < count; ++i)
    {
        thrust[i] += 7.6 * pressure[i];
        burnTime[i] = std::abs(burnTime[i]) + 8.0;
        mixture[i] += 2.4;
    }
    // [series-bubbles]
    rocketplot::ScatterSeries* firings = plot->addScatter(pressure, thrust, "Test firings");

    // A third value as the marker's area: pass diameters, so the square root of the value.
    std::vector<double> diameters;
    diameters.reserve(burnTime.size());
    for (const double seconds : burnTime)
    {
        diameters.push_back(4.0 * std::sqrt(seconds));
    }
    firings->setSizes(diameters);

    // A fourth as its color: one hue from light to dark, low to high.
    std::vector<QColor> colors;
    colors.reserve(mixture.size());
    for (const double ratio : mixture)
    {
        const double level = std::clamp((ratio - 1.8) / 1.2, 0.0, 1.0);
        colors.push_back(QColor::fromHslF(0.59F, 0.75F, static_cast<float>(0.82 - (0.52 * level))));
    }
    firings->setColors(colors);
    // [/series-bubbles]
    plot->xAxis()->setLabel("Chamber pressure (bar)");
    plot->yAxis()->setLabel("Thrust (kN)");
    plot->setTitle("Size: burn time. Shade: mixture ratio");
}

void errors(PlotWidget* plot)
{
    const std::vector<double> time     = linspace(0.0, 60.0, 121);
    const std::vector<double> estimate = settling(time, 2.0, 9.0, 18.0, 0.0, 1);
    std::vector<double>       sigma(time.size());
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        sigma[i] = 0.3 + (0.02 * time[i]);
    }
    const std::vector<double> checkTime{10.0, 20.0, 30.0, 40.0, 50.0};
    const std::vector<double> checkValue{5.2, 6.4, 7.9, 8.1, 8.9};
    const std::vector<double> below{0.6, 0.5, 0.9, 0.4, 0.7};
    const std::vector<double> above{0.3, 0.8, 0.4, 0.9, 0.5};
    // [series-errors]
    // The same error below and above each point. A line shows y errors as a band.
    plot->addLine(time, estimate, "Estimate")->setYErrors(sigma);

    // Different errors below and above. A scatter series shows them as bars.
    rocketplot::ScatterSeries* checks = plot->addScatter(checkTime, checkValue, "Spot checks");
    checks->setYErrors(below, above);
    checks->setXErrors(std::vector<double>(checkTime.size(), 1.5));  // and along x
    // [/series-errors]
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Flow (kg/s)");
    plot->legend()->setAnchor(LegendAnchor::BOTTOM_RIGHT);
}

void styling(const QString& /*scratch*/)
{
    PlotWidget                window;
    PlotWidget*               plot = &window;
    const std::vector<double> x{0.0, 1.0, 2.0};
    const std::vector<double> y{1.0, 3.0, 2.0};
    // [series-color]
    rocketplot::LineSeries* line = plot->addLine(x, y, "Tank pressure");
    line->setColor(QColor("#8e44ad"));  // this color, whatever the theme
    line->resetColor();                 // back to the theme's color for this series
    // [/series-color]

    // [series-errors-style]
    line->setYErrors(std::vector<double>{0.2, 0.3, 0.2});
    line->setErrorStyle(rocketplot::ErrorStyle::BARS);  // bars on a line; BAND is its default
    line->setErrorCapSize(10.0);                        // width of the caps; 0 for none
    line->setBandOpacity(0.25);                         // of a band: 0 to 1
    line->clearErrors();
    // [/series-errors-style]

    // [series-visible]
    line->setVisible(false);  // not drawn, not autoscaled to; its legend entry is dimmed
    line->setName("Tank pressure (bar)");
    plot->removeSeries(line);  // gone for good: the pointer is dangling now
    plot->clearSeries();       // all of them
    // [/series-visible]
}

}  // namespace

void addSeriesExamples(Examples& examples)
{
    examples.addPlot(QStringLiteral("series-lines"), lines);
    examples.addPlot(QStringLiteral("series-markers"), markers, {720, 320});
    examples.addPlot(QStringLiteral("series-bubbles"), bubbles);
    examples.addPlot(QStringLiteral("series-errors"), errors);
    examples.demonstrations.push_back({.name = QStringLiteral("styling a series"), .run = styling});
}

}  // namespace rocketplot::guide
