#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "DemoPage.h"
#include "Generators.h"
#include "rocketplot/Axis.h"
#include "rocketplot/BoxSeries.h"
#include "rocketplot/BoxStatistics.h"
#include "rocketplot/HistogramSeries.h"
#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/enums.h"

namespace rocketplot::demo
{

namespace
{

// The value below which @p share of @p sorted lies (linear between the two nearest).
double percentile(const std::vector<double>& sorted, double share)
{
    const double      position = share * static_cast<double>(sorted.size() - 1);
    const auto        below    = static_cast<std::size_t>(std::floor(position));
    const std::size_t above    = std::min(below + 1, sorted.size() - 1);
    const double      part     = position - static_cast<double>(below);
    return sorted[below] + (part * (sorted[above] - sorted[below]));
}

QWidget* create(QWidget* parent)
{
    auto* page     = new QWidget(parent);
    auto* layout   = new QVBoxLayout(page);
    auto* controls = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    auto* bins = new QComboBox(page);
    for (const int count : {0, 10, 20, 40, 80})
    {
        bins->addItem(count == 0 ? QStringLiteral("Automatic") : QString::number(count), count);
    }
    auto* heights = new QComboBox(page);
    heights->addItem(QStringLiteral("Count"), QVariant::fromValue(HistogramNormalization::COUNT));
    heights->addItem(QStringLiteral("Probability"),
                     QVariant::fromValue(HistogramNormalization::PROBABILITY));
    heights->addItem(QStringLiteral("Density"),
                     QVariant::fromValue(HistogramNormalization::DENSITY));
    controls->addWidget(new QLabel(QStringLiteral("Bins:"), page));
    controls->addWidget(bins);
    controls->addWidget(new QLabel(QStringLiteral("Heights:"), page));
    controls->addWidget(heights);
    controls->addStretch(1);
    layout->addLayout(controls);

    // [snippet]
    // A histogram over a box plot of the same runs, on one apogee axis.
    auto* grid = new rocketplot::PlotGrid(2, 1, page);
    grid->setTitle("Apogee of 2000 simulated flights");
    grid->setRowStretch(0, 2);
    rocketplot::PlotWidget* plot  = grid->plot(0);
    rocketplot::PlotWidget* boxes = grid->plot(1);
    plot->yAxis()->setLabel("Flights");
    boxes->xAxis()->setLabel("Apogee (m)");

    // A Monte Carlo of the flight: each run's apogee, counted into bins. A tenth of the motors
    // burn short.
    const std::vector<double> nominal = gaussian(1800, 3050.0, 60.0, 11);
    const std::vector<double> weak    = gaussian(200, 2880.0, 45.0, 12);
    std::vector<double>       apogee  = nominal;
    apogee.insert(apogee.end(), weak.begin(), weak.end());
    auto* flights = plot->addHistogram(apogee, "Flights");

    // The middle 90% of the runs (labelled at the top, clear of the bars), and the nominal and
    // the median apogee.
    std::vector<double> sorted = apogee;
    std::ranges::sort(sorted);
    plot->addVerticalSpan(percentile(sorted, 0.05), percentile(sorted, 0.95),
                          "5th to 95th percentile")
        ->setLabelAlignment(Qt::AlignTop | Qt::AlignLeft);
    plot->addVerticalLine(3000.0, "Nominal");
    plot->addVerticalLine(percentile(sorted, 0.5), "Median");

    // Below, a box for each kind of run, lying down on a category axis.
    boxes->yAxis()->setScaleType(rocketplot::ScaleType::CATEGORY);
    boxes->yAxis()->setCategories({"All runs", "Full burn", "Short burn"});
    const std::array<const std::vector<double>*, 3> runs{&apogee, &nominal, &weak};
    for (std::size_t category = 0; category < runs.size(); ++category)
    {
        rocketplot::BoxSeries* box =
            boxes->addBox(rocketplot::BoxStatistics::fromValues(*runs.at(category)),
                          static_cast<double>(category));
        box->setOrientation(Qt::Horizontal);
        box->setMeanVisible(true);
    }
    // [/snippet]

    QObject::connect(bins, &QComboBox::currentIndexChanged, flights,
                     [=] { flights->setBinCount(bins->currentData().toInt()); });
    QObject::connect(heights, &QComboBox::currentIndexChanged, flights, [=] {
        const auto normalization = heights->currentData().value<HistogramNormalization>();
        flights->setNormalization(normalization);
        switch (normalization)
        {
            case HistogramNormalization::COUNT:
                plot->yAxis()->setLabel("Flights");
                break;
            case HistogramNormalization::PROBABILITY:
                plot->yAxis()->setLabel("Share of the flights");
                break;
            case HistogramNormalization::DENSITY:
                plot->yAxis()->setLabel("Share per meter");
                break;
        }
    });
    layout->addWidget(grid, 1);
    return page;
}

}  // namespace

DemoPage statisticsPage()
{
    return {
        .title       = QStringLiteral("Statistics"),
        .description = QStringLiteral(
            "A histogram counts values into equal bins: as many as the square root of the count "
            "(5 to 50) unless you choose, and heights as counts, shares of the values or a density "
            "whose area is 1. Point at a bar for its bin and height. Shaded: the middle 90% of the "
            "runs, from their percentiles. Below, a box and whiskers of each kind of run on a "
            "category axis, sharing the apogee axis: the quartiles and the median, whiskers to 1.5 "
            "times the box, outliers beyond them and the mean as a diamond."),
        .sourceFile = QStringLiteral("StatisticsPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
