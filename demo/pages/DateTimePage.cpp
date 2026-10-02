#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"
#include "rocketplot/plottime.h"

namespace rocketplot::demo
{

namespace
{

rocketplot::PlotWidget* createPlot(QWidget* parent)
{
    // [snippet]
    auto* plot = new rocketplot::PlotWidget(parent);
    plot->setTitle("Launch site weather");
    plot->yAxis()->setLabel("Temperature (°C)");
    plot->xAxis()->setScaleType(rocketplot::ScaleType::DATE_TIME);

    // Three days at one sample a minute, as seconds since the epoch. The range includes the night
    // Central European clocks go forward (2026-03-29): try the Europe/Berlin zone.
    const QDateTime     start(QDate(2026, 3, 27), QTime(12, 0), QTimeZone::utc());
    std::vector<double> time;
    std::vector<double> temperature = gaussian(std::size_t{3} * 24 * 60, 0.0, 0.3, 11);
    for (std::size_t i = 0; i < temperature.size(); ++i)
    {
        time.push_back(rocketplot::toPlotTime(start) + (60.0 * static_cast<double>(i)));
        const double day = (static_cast<double>(i) / (24.0 * 60.0)) * 2.0 * std::numbers::pi;
        temperature[i] += 9.0 + (6.0 * std::sin(day - 1.2)) + (0.5 * std::sin(day * 7.0));
    }
    plot->addLine(time, temperature, "Pad 39B");
    // [/snippet]
    return plot;
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* controls = new QHBoxLayout;
    auto* zone     = new QComboBox(page);
    zone->addItem(QStringLiteral("UTC"), QByteArray("UTC"));
    zone->addItem(QStringLiteral("Local time"), QTimeZone::systemTimeZoneId());
    zone->addItem(QStringLiteral("Europe/Berlin"), QByteArray("Europe/Berlin"));
    zone->addItem(QStringLiteral("America/New_York"), QByteArray("America/New_York"));
    controls->addWidget(new QLabel(QStringLiteral("Time zone:"), page));
    controls->addWidget(zone);
    controls->addStretch(1);
    layout->addLayout(controls);
    auto* plot = createPlot(page);
    layout->addWidget(plot, 1);
    QObject::connect(zone, &QComboBox::currentIndexChanged, page, [plot, zone] {
        plot->xAxis()->setTimeZone(QTimeZone(zone->currentData().toByteArray()));
    });
    return page;
}

}  // namespace

DemoPage dateTimePage()
{
    return {
        .title       = QStringLiteral("Date & time"),
        .description = QStringLiteral("A DATE_TIME axis takes seconds since the Unix epoch and "
                                      "puts ticks on calendar boundaries: "
                                      "days, hours, minutes, down to milliseconds. Labels only "
                                      "show what changes; the rest (here "
                                      "the date) is written once under the axis, with the time "
                                      "zone. Zoom in and watch them adapt."),
        .sourceFile  = QStringLiteral("DateTimePage.cpp"),
        .create      = create,
    };
}

}  // namespace rocketplot::demo
