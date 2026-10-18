#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <cstddef>
#include <utility>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/ColorScale.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
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
    auto* maps = new QComboBox(page);
    for (const auto& [name, map] : {std::pair(QStringLiteral("Viridis"), ColorMap::VIRIDIS),
                                    std::pair(QStringLiteral("Cividis"), ColorMap::CIVIDIS),
                                    std::pair(QStringLiteral("Plasma"), ColorMap::PLASMA),
                                    std::pair(QStringLiteral("Inferno"), ColorMap::INFERNO),
                                    std::pair(QStringLiteral("Magma"), ColorMap::MAGMA),
                                    std::pair(QStringLiteral("Gray"), ColorMap::GRAY),
                                    std::pair(QStringLiteral("Cool to warm"), ColorMap::COOL_WARM),
                                    std::pair(QStringLiteral("Blue to red"), ColorMap::BLUE_RED)})
    {
        maps->addItem(name, QVariant::fromValue(map));
    }
    auto* fixed = new QCheckBox(QStringLiteral("Range 0 to 20 m/s"), page);
    fixed->setToolTip(QStringLiteral("Off: the range fits the values"));
    controls->addWidget(new QLabel(QStringLiteral("Colors:"), page));
    controls->addWidget(maps);
    controls->addWidget(fixed);
    controls->addStretch(1);
    layout->addLayout(controls);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle("Landing dispersion by wind");
    plot->xAxis()->setLabel("Crossrange (m)");
    plot->yAxis()->setLabel("Downrange (m)");

    // 800 simulated landings: the stronger the wind, the further they drift downwind.
    constexpr std::size_t     kCount = 800;
    const std::vector<double> wind   = uniform(kCount, 0.0, 18.0, 31);
    const std::vector<double> gustX  = gaussian(kCount, 0.0, 1.0, 32);
    const std::vector<double> gustY  = gaussian(kCount, 0.0, 1.0, 33);
    std::vector<double>       crossrange;
    std::vector<double>       downrange;
    for (std::size_t i = 0; i < kCount; ++i)
    {
        crossrange.push_back(gustX[i] * (8.0 + (1.5 * wind[i])));
        downrange.push_back((3.0 * wind[i]) + (gustY[i] * (8.0 + (2.0 * wind[i]))));
    }
    auto* landings = plot->addScatter(crossrange, downrange, "Landings");

    // A fourth value as color: the plot's color scale maps it, and shows a key.
    landings->setColorValues(wind);
    plot->colorScale()->setLabel("Wind at landing (m/s)");
    plot->setAspectRatio(1.0);  // meters across as long as meters down
    // [/snippet]

    QObject::connect(maps, &QComboBox::currentIndexChanged, plot, [=] {
        plot->colorScale()->setColorMap(maps->currentData().value<ColorMap>());
    });
    QObject::connect(fixed, &QCheckBox::toggled, plot, [plot](bool on) {
        if (on)
        {
            plot->colorScale()->setRange(0.0, 20.0);
        }
        else
        {
            plot->colorScale()->setAutoscale(true);
        }
    });
    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage colorScalePage()
{
    return {
        .title       = QStringLiteral("Color scale"),
        .description = QStringLiteral(
            "Scatter points can carry a value shown as their color: the plot's color scale maps it "
            "and draws a key right of the plot. It fits the values unless given a range; the maps "
            "from dark to light read in order even in gray, the diverging ones go both ways from "
            "a light middle."),
        .sourceFile = QStringLiteral("ColorScalePage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
