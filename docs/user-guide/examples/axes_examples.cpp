// Examples of axes.md.

#include <QDate>
#include <QDateTime>
#include <QLabel>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTime>
#include <QTimeZone>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"
#include "rocketplot/plottime.h"

namespace rocketplot::guide
{

namespace
{

void ranges(const QString& /*scratch*/)
{
    PlotWidget   window;
    PlotWidget*  plot   = &window;
    const Ascent ascent = sampleAscent();
    plot->addLine(ascent.time, ascent.altitude);
    // [axes-range]
    rocketplot::Axis* x = plot->xAxis();
    x->setRange(100.0, 200.0);  // show this stretch; turns autoscale off for the axis
    const rocketplot::Range shown = x->range();  // {.min = 100, .max = 200}

    x->setAutoscale(true);  // follow the data again
    plot->resetView();      // or: every axis back to autoscale
    // [/axes-range]
    static_cast<void>(shown);

    // [axes-autoscale]
    // All the data, with 10% of its span left free on each side (3% by default).
    plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_ALL);
    plot->yAxis()->setAutoscaleMargin(0.10);

    // y fits what is inside the current x range: zoom into a time span and y follows.
    plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);

    // x shows the newest 30 units of data and scrolls as more is appended.
    plot->xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
    plot->xAxis()->setFollowWindow(30.0);
    // [/axes-autoscale]

    QLabel  label;
    QLabel* rangeLabel = &label;
    // [axes-signals]
    QObject::connect(plot->xAxis(), &rocketplot::Axis::rangeChanged, rangeLabel,
                     [rangeLabel](double min, double max) {
                         // The user panned or zoomed, autoscale moved the axis, or code set it.
                         rangeLabel->setText(QString("%1 s to %2 s").arg(min).arg(max));
                     });
    // [/axes-signals]

    plot->resize(600, 400);
    // [axes-mapping]
    // From a position in the widget to data coordinates, and back.
    const QPointF data  = plot->mapToData(QPointF(300.0, 200.0));
    const QPointF pixel = plot->mapFromData(QPointF(150.0, 60.0));
    const QRectF  area  = plot->plotArea();  // where data is drawn, in widget coordinates
    // [/axes-mapping]
    static_cast<void>(data);
    static_cast<void>(pixel);
    static_cast<void>(area);
}

void logScale(PlotWidget* plot)
{
    // The magnitude of a low-pass filter with its corner at 1 kHz.
    const std::size_t   count = 200;
    std::vector<double> frequency(count);
    std::vector<double> gain(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        frequency[i] =
            std::pow(10.0, 1.0 + (4.0 * static_cast<double>(i) / static_cast<double>(count - 1)));
        gain[i] = 1.0 / std::sqrt(1.0 + std::pow(frequency[i] / 1000.0, 4.0));
    }
    // [axes-log]
    plot->addLine(frequency, gain);
    plot->xAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    plot->yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    plot->xAxis()->setMinorGridVisible(true);  // the 2, 3, ... 9 between the powers of ten
    // [/axes-log]
    plot->setTitle("Low-pass filter");
    plot->xAxis()->setLabel("Frequency (Hz)");
    plot->yAxis()->setLabel("Gain");
}

void dateTime(PlotWidget* plot)
{
    // A reading a minute for a day and a half, from a start time.
    const QDateTime     start(QDate(2026, 3, 28), QTime(18, 0), QTimeZone::UTC);
    const std::size_t   count = 2160;  // minutes
    std::vector<double> seconds(count);
    std::vector<double> temperature = noise(count, 0.15, 12);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double hours = static_cast<double>(i) / 60.0;
        seconds[i]         = rocketplot::toPlotTime(start) + (hours * 3600.0);
        temperature[i] += 14.0 + (6.0 * std::sin(2.0 * std::numbers::pi * (hours - 3.0) / 24.0));
    }
    // [axes-time]
    // x values are seconds since 1970-01-01 00:00 UTC, as a double.
    plot->addLine(seconds, temperature);
    plot->xAxis()->setScaleType(rocketplot::ScaleType::DATE_TIME);
    plot->xAxis()->setTimeZone(QTimeZone("Europe/Berlin"));  // UTC unless told otherwise
    // [/axes-time]
    plot->setTitle("Hangar temperature");
    plot->yAxis()->setLabel("Temperature (°C)");
}

void timeConversions(const QString& /*scratch*/)
{
    // [axes-time-values]
    const QDateTime liftoff(QDate(2026, 3, 28), QTime(18, 30), QTimeZone::UTC);
    const double    x    = rocketplot::toPlotTime(liftoff);  // what the axis takes
    const QDateTime back = rocketplot::fromPlotTime(x);      // and back, in UTC
    // [/axes-time-values]
    static_cast<void>(back);
}

void twoAxes(PlotWidget* plot)
{
    const Ascent              ascent = sampleAscent();
    const std::vector<double> time   = ascent.time;
    // [axes-two]
    plot->addLine(time, ascent.altitude, "Altitude");
    plot->yAxis()->setLabel("Altitude (km)");

    // The second series is measured in other units: it gets the axis on the right.
    rocketplot::LineSeries* velocity = plot->addLine(time, ascent.velocity, "Velocity");
    velocity->setYAxis(plot->yAxis2());
    plot->yAxis2()->setLabel("Velocity (m/s)");
    // [/axes-two]
    plot->xAxis()->setLabel("Time (s)");
    plot->legend()->setAnchor(LegendAnchor::TOP_LEFT);
}

void siPrefixes(PlotWidget* plot)
{
    // A capacitor discharging through a resistor: 2.2 µF, 1 kΩ.
    const std::vector<double> time = linspace(0.0, 0.012, 241);
    std::vector<double>       current(time.size());
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        current[i] = 0.005 * std::exp(-time[i] / 0.0022);
    }
    // [axes-si]
    plot->addLine(time, current);
    plot->xAxis()->setNumberFormat(rocketplot::NumberFormat::SI);  // 2m, 4m, ... for 0.002, 0.004
    plot->yAxis()->setNumberFormat(rocketplot::NumberFormat::SI);
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Current (A)");
    // [/axes-si]
    plot->setTitle("NumberFormat::SI");
}

void offsetLabels(PlotWidget* plot)
{
    // A frequency that wanders a few hertz around 1.42 GHz.
    const std::vector<double> time      = linspace(0.0, 60.0, 601);
    std::vector<double>       frequency = noise(time.size(), 0.6, 21);
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        frequency[i] += 1'420'405'751.0 + (4.0 * std::sin(time[i] / 9.0));
    }
    // [axes-offset]
    plot->addLine(time, frequency);  // values around 1 420 405 751, a few apart
    plot->yAxis()->setLabel("Frequency (Hz)");
    // [/axes-offset]
    plot->xAxis()->setLabel("Time (s)");
    plot->setTitle("NumberFormat::AUTO");
}

void grid(const QString& /*scratch*/)
{
    const PlotWidget  window;
    const PlotWidget* plot = &window;
    // [axes-grid]
    plot->xAxis()->setGridVisible(false);      // no vertical grid lines
    plot->yAxis()->setMinorGridVisible(true);  // fainter lines at the minor ticks too
    plot->yAxis2()->setGridVisible(true);      // the right axis has no grid by default

    plot->yAxis()->setVisible(false);            // no axis line, ticks or labels at all
    plot->xAxis()->setTickLabelsVisible(false);  // the line and ticks, but no numbers
    // [/axes-grid]
}

}  // namespace

void addAxesExamples(Examples& examples)
{
    examples.demonstrations.push_back({.name = QStringLiteral("ranges"), .run = ranges});
    examples.demonstrations.push_back(
        {.name = QStringLiteral("time values"), .run = timeConversions});
    examples.demonstrations.push_back({.name = QStringLiteral("grid"), .run = grid});
    examples.addPlot(QStringLiteral("axes-log"), logScale);
    examples.addPlot(QStringLiteral("axes-time"), dateTime);
    examples.addPlot(QStringLiteral("axes-two"), twoAxes);
    examples.addPlot(QStringLiteral("axes-si"), siPrefixes, {720, 340});
    examples.addPlot(QStringLiteral("axes-offset"), offsetLabels, {720, 340});
}

}  // namespace rocketplot::guide
