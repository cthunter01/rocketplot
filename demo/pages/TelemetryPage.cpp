#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "DemoPage.h"
#include "TelemetrySimulator.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

constexpr int    kTickMilliseconds = 33;
constexpr double kLongestTick      = 0.25;  // s: after a stall, don't fly it all at once

// A telemetry channel: one plot of the stack.
struct Channel
{
    const char* name;
    const char* label;  // of its y axis
    double TelemetrySample::* reading;
    bool                      shownAtFirst;
};
constexpr std::array kChannels{
    Channel{
        .name         = "Altitude",
        .label        = "Altitude (km)",
        .reading      = &TelemetrySample::altitude,
        .shownAtFirst = true,
    },
    Channel{
        .name         = "Velocity",
        .label        = "Velocity (m/s)",
        .reading      = &TelemetrySample::velocity,
        .shownAtFirst = true,
    },
    Channel{
        .name         = "Acceleration",
        .label        = "Accel. (g)",
        .reading      = &TelemetrySample::acceleration,
        .shownAtFirst = true,
    },
    Channel{
        .name         = "Dynamic pressure",
        .label        = "q (kPa)",
        .reading      = &TelemetrySample::dynamicPressure,
        .shownAtFirst = false,
    },
    Channel{
        .name         = "Downrange",
        .label        = "Downrange (km)",
        .reading      = &TelemetrySample::downrange,
        .shownAtFirst = false,
    },
};

// The flight and the plots that show it.
struct Flight
{
    TelemetrySimulator                                    simulator;
    std::array<rocketplot::PlotWidget*, kChannels.size()> plots{};
    std::array<rocketplot::LineSeries*, kChannels.size()> series{};
    QElapsedTimer                                         clock;  // since the last tick
    double          speed = 10.0;                                 // flight seconds per second
    TelemetrySample latest;
};

// Flies on for @p seconds and plots what the vehicle reported on the way.
void fly(Flight& flight, double seconds)
{
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent>  events;
    flight.simulator.advance(seconds, samples, events);
    if (samples.empty())
    {
        return;
    }
    flight.latest = samples.back();
    // [snippet]
    // One append() per channel and tick, however many readings arrived: one repaint each.
    std::vector<double> time(samples.size());
    std::vector<double> readings(samples.size());
    std::ranges::transform(samples, time.begin(), &TelemetrySample::time);
    for (std::size_t channel = 0; channel < kChannels.size(); ++channel)
    {
        std::ranges::transform(samples, readings.begin(), kChannels.at(channel).reading);
        flight.series.at(channel)->append(time, readings);
    }
    // What happened on the way: named on the first plot, a plain line on the others.
    for (const TelemetryEvent& event : events)
    {
        const QString name = QString::fromUtf8(event.name);
        flight.plots.front()->addEvent(event.time, name);
        for (std::size_t channel = 1; channel < kChannels.size(); ++channel)
        {
            flight.plots.at(channel)->addVerticalLine(event.time);
        }
    }
    // [/snippet]
}

QString readout(const Flight& flight)
{
    const QLocale locale;
    if (flight.simulator.time() <= 0.0)
    {
        return QStringLiteral("On the pad");
    }
    return QStringLiteral("T+%1 s    %2 km    %3 m/s%4")
        .arg(locale.toString(flight.latest.time, 'f', 1),
             locale.toString(flight.latest.altitude, 'f', 1),
             locale.toString(flight.latest.velocity, 'f', 0),
             flight.simulator.isInOrbit() ? QStringLiteral("    In orbit") : QString());
}

// The time axis is named once, under the lowest plot that is shown.
void labelTimeAxis(const Flight& flight)
{
    bool labeled = false;
    for (std::size_t channel = kChannels.size(); channel-- > 0;)
    {
        const rocketplot::PlotWidget* plot = flight.plots.at(channel);
        const bool                    last = !labeled && !plot->isHidden();
        plot->xAxis()->setLabel(last ? QStringLiteral("Mission elapsed time (s)") : QString());
        labeled = labeled || last;
    }
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* controls = new QHBoxLayout;
    auto* run      = new QPushButton(QStringLiteral("Pause"), page);
    auto* speed    = new QComboBox(page);
    speed->addItem(QStringLiteral("Real time"), 1.0);
    speed->addItem(QStringLiteral("10× faster"), 10.0);
    speed->addItem(QStringLiteral("60× faster"), 60.0);
    speed->setCurrentIndex(1);
    auto* view = new QComboBox(page);
    view->addItem(QStringLiteral("Whole flight"), 0.0);
    view->addItem(QStringLiteral("Last 60 s"), 60.0);
    view->addItem(QStringLiteral("Last 10 s"), 10.0);
    auto* status = new QLabel(page);
    controls->addWidget(run);
    controls->addWidget(speed);
    controls->addWidget(new QLabel(QStringLiteral("Show:"), page));
    controls->addWidget(view);
    layout->addLayout(controls);

    const auto flight = std::make_shared<Flight>();
    auto*      stack  = new QVBoxLayout;
    stack->setSpacing(0);
    auto* link = new rocketplot::PlotLink(page);
    for (std::size_t channel = 0; channel < kChannels.size(); ++channel)
    {
        const Channel& described = kChannels.at(channel);
        auto*          plot      = new rocketplot::PlotWidget(page);
        plot->yAxis()->setLabel(QString::fromLatin1(described.label));
        plot->setCrosshairEnabled(true);
        flight->plots.at(channel)  = plot;
        flight->series.at(channel) = plot->addLine(std::vector<double>{}, std::vector<double>{});
        link->addPlot(plot);
        stack->addWidget(plot, 1);
        if (channel == 0)
        {
            continue;  // the first plot names the events: it stays
        }
        auto* shown = new QCheckBox(QString::fromLatin1(described.name), page);
        shown->setChecked(described.shownAtFirst);
        plot->setVisible(described.shownAtFirst);
        controls->addWidget(shown);
        QObject::connect(shown, &QCheckBox::toggled, page, [flight, plot](bool checked) {
            plot->setVisible(checked);
            labelTimeAxis(*flight);
        });
    }
    controls->addWidget(status, 1);
    layout->addLayout(stack, 1);
    labelTimeAxis(*flight);

    const auto applyView = [flight, view] {
        // [snippet]

        // Mission control's strip chart: the time axis shows the newest stretch and scrolls,
        // and each plot fits what is in view. Linked plots share the range, not the mode.
        const double window = view->currentData().toDouble();
        for (rocketplot::PlotWidget* plot : flight->plots)
        {
            if (window > 0.0)
            {
                plot->xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
                plot->xAxis()->setFollowWindow(window);
                plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
            }
            else
            {
                plot->xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_ALL);
                plot->yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_ALL);
            }
            plot->resetView();
        }
        // [/snippet]
    };
    applyView();
    QObject::connect(view, &QComboBox::currentIndexChanged, page, applyView);

    auto* timer = new QTimer(page);
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, page, [flight, timer, run, status] {
        const double elapsed = static_cast<double>(flight->clock.restart()) / 1000.0;
        fly(*flight, std::min(elapsed, kLongestTick) * flight->speed);
        status->setText(readout(*flight));
        if (flight->simulator.isInOrbit())
        {
            timer->stop();
            run->setText(QStringLiteral("Launch again"));
        }
    });
    QObject::connect(run, &QPushButton::clicked, page, [flight, timer, run] {
        if (flight->simulator.isInOrbit())
        {
            // Back to the pad: empty plots, and a new flight.
            flight->simulator = TelemetrySimulator();
            for (std::size_t channel = 0; channel < kChannels.size(); ++channel)
            {
                flight->series.at(channel)->clear();
                flight->plots.at(channel)->clearAnnotations();
                flight->plots.at(channel)->resetView();
            }
        }
        else if (timer->isActive())
        {
            timer->stop();
            run->setText(QStringLiteral("Resume"));
            return;
        }
        flight->clock.restart();
        timer->start(kTickMilliseconds);
        run->setText(QStringLiteral("Pause"));
    });
    QObject::connect(speed, &QComboBox::currentIndexChanged, page,
                     [flight, speed] { flight->speed = speed->currentData().toDouble(); });

    status->setText(readout(*flight));
    flight->clock.start();
    timer->start(kTickMilliseconds);
    return page;
}

}  // namespace

DemoPage telemetryPage()
{
    return {
        .title       = QStringLiteral("Telemetry"),
        .description = QStringLiteral(
            "A simulated launch, reported live: a two-stage vehicle flies to orbit in about nine "
            "minutes, and its channels arrive twenty times a second of flight. Each channel has "
            "its own plot on a shared time axis; events are marked as they happen. Show the last "
            "minute to follow the flight like a strip chart, pan or zoom to look back while it "
            "goes on (double-click to follow again), and point at a plot to read every channel "
            "at that time."),
        .sourceFile = QStringLiteral("TelemetryPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
