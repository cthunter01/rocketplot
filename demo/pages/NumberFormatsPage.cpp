#include <QGridLayout>
#include <QString>
#include <QWidget>
#include <cstddef>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

QWidget* create(QWidget* parent)
{
    auto* page = new QWidget(parent);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    // [snippet]
    // Large values with a small range: an offset ("+1.7×10⁹") keeps the labels short.
    auto*               gps  = new rocketplot::PlotWidget(page);
    std::vector<double> week = randomWalk(500, 0.002, 21);
    std::vector<double> clock(week.size());
    for (std::size_t i = 0; i < week.size(); ++i)
    {
        clock[i] = 1.7e9 + (static_cast<double>(i) * 0.002);
        week[i] += 4.2e5;
    }
    gps->setTitle("GPS seconds");
    gps->addLine(clock, week);

    // Tiny values: a ×10⁻⁶ multiplier...
    auto*                     tiny    = new rocketplot::PlotWidget(page);
    const std::vector<double> current = gaussian(400, 2.5e-6, 1e-7, 22);
    tiny->setTitle("Leakage current (A)");
    tiny->addLine(current);

    // ...or SI prefixes on every label.
    auto* si = new rocketplot::PlotWidget(page);
    si->setTitle("Leakage current (A), SI");
    si->yAxis()->setNumberFormat(rocketplot::NumberFormat::SI);
    si->addLine(current);

    // Rich text in titles, axis labels and legend names.
    auto*                     rich  = new rocketplot::PlotWidget(page);
    const std::vector<double> drift = randomWalk(400, 0.05, 23);
    rich->setTitle("v<sub>z</sub> after T<sub>0</sub>");
    rich->xAxis()->setLabel("<i>t</i> − T<sub>0</sub> (s)");
    rich->yAxis()->setLabel("v<sub>z</sub> (m s<sup>−1</sup>)");
    rich->addLine(drift);
    // [/snippet]
    grid->addWidget(gps, 0, 0);
    grid->addWidget(tiny, 0, 1);
    grid->addWidget(si, 1, 0);
    grid->addWidget(rich, 1, 1);
    return page;
}

}  // namespace

DemoPage numberFormatsPage()
{
    return {
        .title = QStringLiteral("Number formats"),
        .description =
            QStringLiteral("Tick labels stay short: a common offset when many leading digits "
                           "repeat, a ×10ⁿ multiplier "
                           "for very large or small numbers, or SI prefixes per label. Titles, "
                           "axis labels and legend "
                           "names accept Qt rich text (subscripts, superscripts, italics)."),
        .sourceFile = QStringLiteral("NumberFormatsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
