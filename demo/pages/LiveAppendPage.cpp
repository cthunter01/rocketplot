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
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

constexpr int kTickMilliseconds = 10;

// Three channels doing random walks, sampled in real time.
struct Feed
{
    std::array<rocketplot::LineSeries*, 3> series{};
    std::array<double, 3>                  values{};
    std::mt19937_64                        engine{std::random_device{}()};
    QElapsedTimer                          clock;
    double                                 rate     = 100.0;  // samples per second
    std::int64_t                           produced = 0;      // samples per channel so far
    double                                 offset   = 0.0;  // seconds before the clock (re)started
};

// Appends the samples that are due by now.
void appendDue(Feed& feed)
{
    const double elapsed = feed.offset + (static_cast<double>(feed.clock.nsecsElapsed()) / 1e9);
    const auto   due = static_cast<std::int64_t>(std::floor(elapsed * feed.rate)) - feed.produced;
    if (due <= 0)
    {
        return;
    }
    std::uniform_real_distribution<double> step(-1.0, 1.0);
    // [snippet]
    // Batch the samples of one tick: one append() (and one repaint) per channel.
    std::vector<double> time(static_cast<std::size_t>(due));
    for (std::size_t i = 0; i < time.size(); ++i)
    {
        time[i] = static_cast<double>(feed.produced + static_cast<std::int64_t>(i)) / feed.rate;
    }
    for (std::size_t channel = 0; channel < feed.series.size(); ++channel)
    {
        std::vector<double> values(time.size());
        for (double& value : values)
        {
            feed.values.at(channel) += step(feed.engine);
            value = feed.values.at(channel);
        }
        feed.series.at(channel)->append(time, values);
    }
    // [/snippet]
    feed.produced += due;
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* controls = new QHBoxLayout;
    auto* start    = new QPushButton(QStringLiteral("Pause"), page);
    start->setCheckable(true);
    start->setChecked(true);
    auto* rate = new QComboBox(page);
    rate->addItem(QStringLiteral("10 Hz"), 10.0);
    rate->addItem(QStringLiteral("100 Hz"), 100.0);
    rate->addItem(QStringLiteral("1 kHz"), 1000.0);
    rate->setCurrentIndex(1);
    auto* clear  = new QPushButton(QStringLiteral("Clear"), page);
    auto* status = new QLabel(page);
    controls->addWidget(start);
    controls->addWidget(new QLabel(QStringLiteral("Rate:"), page));
    controls->addWidget(rate);
    auto* view = new QComboBox(page);
    view->addItem(QStringLiteral("Fit all"), 0.0);
    view->addItem(QStringLiteral("Follow last 10 s"), 10.0);
    view->addItem(QStringLiteral("Follow last 60 s"), 60.0);
    view->setCurrentIndex(1);
    controls->addWidget(new QLabel(QStringLiteral("View:"), page));
    controls->addWidget(view);
    controls->addWidget(clear);
    controls->addWidget(status, 1);
    layout->addLayout(controls);

    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle(QStringLiteral("Live channels"));
    plot->xAxis()->setLabel(QStringLiteral("Time (s)"));
    layout->addWidget(plot, 1);
    const auto applyView = [plot, view] {
        // [snippet]
        // A strip chart: x shows the newest window of data and scrolls; y fits what's in view.
        const double window = view->currentData().toDouble();
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
        // [/snippet]
    };
    applyView();
    QObject::connect(view, &QComboBox::currentIndexChanged, page, applyView);

    const auto feed = std::make_shared<Feed>();
    for (std::size_t channel = 0; channel < feed->series.size(); ++channel)
    {
        feed->series.at(channel) = plot->addLine(std::vector<double>{}, std::vector<double>{},
                                                 QStringLiteral("Channel %1").arg(channel + 1));
    }
    feed->clock.start();

    auto* timer = new QTimer(page);
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, page, [feed, status] {
        appendDue(*feed);
        status->setText(QStringLiteral("%1 samples per channel")
                            .arg(QLocale().toString(static_cast<qlonglong>(feed->produced))));
    });
    QObject::connect(start, &QPushButton::toggled, page, [feed, timer, start](bool running) {
        start->setText(running ? QStringLiteral("Pause") : QStringLiteral("Resume"));
        if (running)
        {
            feed->clock.restart();
            timer->start(kTickMilliseconds);
        }
        else
        {
            feed->offset += static_cast<double>(feed->clock.nsecsElapsed()) / 1e9;
            timer->stop();
        }
    });
    QObject::connect(rate, &QComboBox::currentIndexChanged, page, [feed, rate] {
        // Keep time continuous: restart the count from the current time at the new rate.
        const double now = feed->offset + (static_cast<double>(feed->clock.nsecsElapsed()) / 1e9);
        feed->rate       = rate->currentData().toDouble();
        feed->offset     = now;
        feed->clock.restart();
        feed->produced = static_cast<std::int64_t>(std::floor(now * feed->rate));
    });
    QObject::connect(clear, &QPushButton::clicked, page, [feed, plot] {
        for (rocketplot::LineSeries* series : feed->series)
        {
            series->clear();
        }
        plot->resetView();
    });
    timer->start(kTickMilliseconds);
    return page;
}

}  // namespace

DemoPage liveAppendPage()
{
    return {
        .title       = QStringLiteral("Live append"),
        .description = QStringLiteral(
            "Samples are appended as they arrive. Following the latest data turns the plot into a "
            "strip chart: x shows the newest window and scrolls, y fits what is in view. Pan or "
            "zoom to take over an axis; double-click to hand it back."),
        .sourceFile = QStringLiteral("LiveAppendPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
