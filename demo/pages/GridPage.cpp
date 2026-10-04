#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

constexpr double kFirstStageEnd    = 150.0;  // s: main engine cutoff
constexpr double kSecondStageStart = 160.0;  // s: second-stage ignition

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* controls = new QHBoxLayout;
    auto* shared   = new QComboBox(page);
    shared->addItem(QStringLiteral("Each column shares its time axis"),
                    QVariant::fromValue(rocketplot::GridLink::COLUMNS));
    shared->addItem(QStringLiteral("All plots share one time axis"),
                    QVariant::fromValue(rocketplot::GridLink::ALL));
    shared->addItem(QStringLiteral("Every plot has its own"),
                    QVariant::fromValue(rocketplot::GridLink::NONE));
    auto* inner = new QCheckBox(QStringLiteral("Tick labels on every plot"), page);
    controls->addWidget(shared);
    controls->addWidget(inner);
    controls->addStretch(1);
    layout->addLayout(controls);

    const Ascent ascent = simulateAscent(300.0, 50.0, 7);
    // Where each stage's burn begins and ends in the samples.
    const auto index = [&ascent](double time) {
        return static_cast<std::size_t>(std::ranges::lower_bound(ascent.time, time) -
                                        ascent.time.begin());
    };
    const std::array<std::size_t, 2> first{index(0.0), index(kFirstStageEnd)};
    const std::array<std::size_t, 2> second{index(kSecondStageStart), ascent.time.size()};

    // [snippet]
    // Three channels in rows, the two stages in columns. The grid makes the plots; each is set up
    // like any other.
    auto* grid = new rocketplot::PlotGrid(3, 2, page);
    grid->setTitle("Ascent, stage by stage");
    grid->setCrosshairEnabled(true);
    grid->plot(0, 0)->setTitle("First stage");
    grid->plot(0, 1)->setTitle("Second stage");
    grid->plot(0, 0)->yAxis()->setLabel("Altitude (km)");
    grid->plot(1, 0)->yAxis()->setLabel("Velocity (m/s)");
    grid->plot(2, 0)->yAxis()->setLabel("Accel. (m/s²)");
    const std::array channels{&ascent.altitude, &ascent.velocity, &ascent.acceleration};
    const std::array stages{first, second};
    for (int row = 0; row < grid->rowCount(); ++row)
    {
        for (int column = 0; column < grid->columnCount(); ++column)
        {
            const auto [from, to] = stages.at(static_cast<std::size_t>(column));
            const std::span time(ascent.time);
            const std::span values(*channels.at(static_cast<std::size_t>(row)));
            grid->plot(row, column)
                ->addLine(time.subspan(from, to - from), values.subspan(from, to - from));
        }
    }
    // The bottom row names the axis its column shares; only it writes the times.
    grid->plot(2, 0)->xAxis()->setLabel("Time (s)");
    grid->plot(2, 1)->xAxis()->setLabel("Time (s)");
    // [/snippet]
    layout->addWidget(grid, 1);

    QObject::connect(shared, &QComboBox::currentIndexChanged, grid, [grid, shared] {
        grid->setXLink(shared->currentData().value<rocketplot::GridLink>());
        grid->resetView();
    });
    QObject::connect(inner, &QCheckBox::toggled, grid,
                     &rocketplot::PlotGrid::setInnerTickLabelsVisible);
    return page;
}

}  // namespace

DemoPage gridPage()
{
    return {
        .title       = QStringLiteral("Subplot grid"),
        .description = QStringLiteral(
            "A PlotGrid arranges plots in rows and columns and lines their plot areas up, "
            "however much room each one's labels take. The plots of a column share their x axis "
            "by default: pan or zoom one and the others in its column follow, the crosshair "
            "shows the same time in them, and only the bottom one writes the times. The whole "
            "grid exports as one image or drawing."),
        .sourceFile = QStringLiteral("GridPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
