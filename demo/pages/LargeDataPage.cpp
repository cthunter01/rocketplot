#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

#include "DemoPage.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/UniformX.h"

namespace rocketplot::demo
{

namespace
{

// More than this many points in total would need gigabytes.
constexpr std::size_t kMaxTotalPoints = 20'000'000;

enum class XKind : std::uint8_t
{
    UNIFORM,
    SORTED,
    UNSORTED,
};

// Owns the data of zero-copy views; a child of the page, so it lives exactly as long as the plot.
class DataStore : public QObject
{
public:
    using QObject::QObject;
    std::vector<std::vector<double>> arrays;
};

// A random walk with a slow wave, cheap enough to make 10 million points of quickly.
std::vector<double> signal(std::size_t count, std::uint64_t seed)
{
    std::mt19937_64                        engine(seed);
    std::uniform_real_distribution<double> step(-1.0, 1.0);
    std::vector<double>                    values(count);
    double                                 walk = 0.0;
    const double scale = 2.0 * std::numbers::pi / static_cast<double>(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        walk += step(engine);
        values[i] = walk + (300.0 * std::sin(static_cast<double>(i) * 3.0 * scale));
    }
    return values;
}

void generate(rocketplot::PlotWidget* plot, DataStore* store, int seriesCount,
              std::size_t pointsEach, XKind kind, bool view)
{
    // [snippet]
    plot->clearSeries();  // first: the views read the store's arrays
    store->arrays.clear();
    store->arrays.reserve(
        2 * static_cast<std::size_t>(seriesCount));  // no reallocation while views point in
    for (int s = 0; s < seriesCount; ++s)
    {
        std::vector<double> y = signal(pointsEach, static_cast<std::uint64_t>(s) + 1);
        for (double& v : y)  // keep the series apart a little
        {
            v += 100.0 * s;
        }
        const QString name = QStringLiteral("Series %1").arg(s + 1);
        if (kind == XKind::UNIFORM)
        {
            const rocketplot::UniformX x{.start = 0.0, .step = 1e-3};
            if (view)
            {
                plot->addLineView(x, store->arrays.emplace_back(std::move(y)), name);
            }
            else
            {
                plot->addLine(x, std::move(y), name);
            }
            continue;
        }
        std::vector<double> x(pointsEach);
        for (std::size_t i = 0; i < pointsEach; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(pointsEach);
            // Unsorted: a Lissajous figure, whose x goes back and forth.
            x[i] = kind == XKind::SORTED ? static_cast<double>(i) * 1e-3
                                         : 1e4 * std::sin(2.0 * std::numbers::pi * 7.0 * t);
        }
        if (view)
        {
            const auto& xs = store->arrays.emplace_back(std::move(x));
            plot->addLineView(xs, store->arrays.emplace_back(std::move(y)), name);
        }
        else
        {
            plot->addLine(std::move(x), std::move(y), name);
        }
    }
    // [/snippet]
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* store  = new DataStore(page);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* controls = new QHBoxLayout;
    auto* series   = new QSpinBox(page);
    series->setRange(1, 10);
    series->setValue(1);
    auto* points = new QComboBox(page);
    for (const std::size_t count : {100'000UL, 1'000'000UL, 5'000'000UL, 10'000'000UL})
    {
        points->addItem(QLocale().toString(static_cast<qulonglong>(count)),
                        QVariant::fromValue(static_cast<qulonglong>(count)));
    }
    points->setCurrentIndex(3);
    auto* xKind = new QComboBox(page);
    xKind->addItem(QStringLiteral("Uniform x (implicit)"), static_cast<int>(XKind::UNIFORM));
    xKind->addItem(QStringLiteral("Sorted x array"), static_cast<int>(XKind::SORTED));
    xKind->addItem(QStringLiteral("Unsorted x (Lissajous)"), static_cast<int>(XKind::UNSORTED));
    auto* storage = new QComboBox(page);
    storage->addItem(QStringLiteral("Copied in"), false);
    storage->addItem(QStringLiteral("Zero-copy view"), true);
    auto* button = new QPushButton(QStringLiteral("Generate"), page);
    auto* status = new QLabel(page);
    controls->addWidget(new QLabel(QStringLiteral("Series:"), page));
    controls->addWidget(series);
    controls->addWidget(new QLabel(QStringLiteral("Points each:"), page));
    controls->addWidget(points);
    controls->addWidget(xKind);
    controls->addWidget(storage);
    controls->addWidget(button);
    controls->addWidget(status, 1);
    layout->addLayout(controls);

    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle(QStringLiteral("Large data"));
    plot->xAxis()->setLabel(QStringLiteral("Time (s)"));
    plot->setDebugOverlay(true);
    plot->setProperty("keepOverlay", true);  // the toolbar toggle leaves this page's overlay on
    layout->addWidget(plot, 1);

    const auto run = [=] {
        const std::size_t pointsEach = points->currentData().value<qulonglong>();
        const int         requested  = series->value();
        const int         allowed =
            static_cast<int>(std::max<std::size_t>(1, kMaxTotalPoints / pointsEach));
        const int seriesCount = std::min(requested, allowed);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QElapsedTimer timer;
        timer.start();
        generate(plot, store, seriesCount, pointsEach,
                 static_cast<XKind>(xKind->currentData().toInt()), storage->currentData().toBool());
        QApplication::restoreOverrideCursor();
        const QLocale locale;
        QString       text = QStringLiteral("%1 points in %2 ms")
                                 .arg(locale.toString(static_cast<qulonglong>(pointsEach) *
                                                      static_cast<qulonglong>(seriesCount)))
                                 .arg(timer.elapsed());
        if (seriesCount < requested)
        {
            text += QStringLiteral(" (limited to %1 series)").arg(seriesCount);
        }
        status->setText(text);
    };
    QObject::connect(button, &QPushButton::clicked, page, run);
    run();
    return page;
}

}  // namespace

DemoPage largeDataPage()
{
    return {
        .title       = QStringLiteral("Large data"),
        .description = QStringLiteral(
            "Millions of points per series, with the debug overlay on. A sorted series is drawn "
            "from "
            "its visible index range, as the first, lowest, highest and last point of each pixel "
            "column; an unsorted one visits every point. Pan and zoom, and watch the frame time."),
        .sourceFile = QStringLiteral("LargeDataPage.cpp"),
        .create     = create};
}

}  // namespace rocketplot::demo
