#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
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
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"

namespace rocketplot::demo
{

namespace
{

constexpr double kMaxWind = 20.0;
constexpr QSize  kScaleSize{160, 12};

// A sequential color scale: steps of one hue, so that more reads as more. The low end is the one
// nearer the background (light on light, dark on dark), but stops short of it: every marker has to
// show.
QColor windColor(double wind, bool darkBackground)
{
    static const std::array<QColor, 4> kOnLight{
        QColor(0x86, 0xb6, 0xef),
        QColor(0x39, 0x87, 0xe5),
        QColor(0x1c, 0x5c, 0xab),
        QColor(0x0d, 0x36, 0x6b),
    };
    static const std::array<QColor, 4> kOnDark{
        QColor(0x18, 0x4f, 0x95),
        QColor(0x2a, 0x78, 0xd6),
        QColor(0x6d, 0xa7, 0xec),
        QColor(0xcd, 0xe2, 0xfb),
    };
    const std::array<QColor, 4>& steps = darkBackground ? kOnDark : kOnLight;
    const double                 position =
        std::clamp(wind / kMaxWind, 0.0, 1.0) * static_cast<double>(steps.size() - 1);
    const std::size_t step  = std::min(static_cast<std::size_t>(position), steps.size() - 2);
    const double      share = position - static_cast<double>(step);
    const QColor&     from  = steps.at(step);
    const QColor&     to    = steps.at(step + 1);
    const auto        mix   = [share](int a, int b) {
        return static_cast<int>(std::lround(a + (share * (b - a))));
    };
    return {mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue())};
}

// The scale as a strip, for the key above the plot.
QPixmap scaleStrip(bool darkBackground)
{
    QPixmap strip(kScaleSize);
    strip.fill(Qt::transparent);
    QPainter        painter(&strip);
    QLinearGradient gradient(0.0, 0.0, kScaleSize.width(), 0.0);
    constexpr int   kStops = 8;
    for (int stop = 0; stop <= kStops; ++stop)
    {
        const double share = static_cast<double>(stop) / kStops;
        gradient.setColorAt(share, windColor(share * kMaxWind, darkBackground));
    }
    painter.fillRect(strip.rect(), gradient);
    return strip;
}

QWidget* create(QWidget* parent)
{
    auto* page   = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    auto* key    = new QHBoxLayout;
    auto* strip  = new QLabel(page);
    layout->setContentsMargins(0, 0, 0, 0);
    key->addWidget(new QLabel(QStringLiteral("Color: wind at landing, 0 m/s"), page));
    key->addWidget(strip);
    key->addWidget(new QLabel(QStringLiteral("20 m/s"), page));
    key->addSpacing(24);
    key->addWidget(new QLabel(QStringLiteral("Size: touchdown speed, 1 to 5 m/s"), page));
    key->addStretch(1);
    layout->addLayout(key);

    // [snippet]
    auto* plot = new rocketplot::PlotWidget(page);
    plot->setTitle("Landing dispersion");
    plot->xAxis()->setLabel("Crossrange (m)");
    plot->yAxis()->setLabel("Downrange (m)");

    // 250 simulated landings: stronger wind scatters them further and lands them harder.
    constexpr std::size_t     kCount = 250;
    const std::vector<double> wind   = uniform(kCount, 0.0, kMaxWind, 1);
    const std::vector<double> gustX  = gaussian(kCount, 0.0, 1.0, 2);
    const std::vector<double> gustY  = gaussian(kCount, 0.0, 1.0, 3);
    const std::vector<double> bounce = gaussian(kCount, 0.0, 0.4, 4);
    std::vector<double>       x;
    std::vector<double>       y;
    std::vector<double>       sizes;
    for (std::size_t i = 0; i < kCount; ++i)
    {
        x.push_back(gustX[i] * (6.0 + (2.5 * wind[i])));
        y.push_back(gustY[i] * (6.0 + (2.5 * wind[i])));
        const double touchdownSpeed = 1.5 + (0.12 * wind[i]) + std::abs(bounce[i]);
        sizes.push_back(4.0 * touchdownSpeed);  // a marker diameter in pixels
    }
    auto* landings = plot->addScatter(x, y);

    // A third and a fourth value per point: its marker's size and color.
    landings->setSizes(sizes);
    const auto recolor = [=] {
        // The colors are yours, so following the theme is too.
        const bool          dark = plot->theme().background.lightnessF() < 0.5F;
        std::vector<QColor> colors;
        colors.reserve(wind.size());
        for (const double speed : wind)
        {
            colors.push_back(windColor(speed, dark));
        }
        landings->setColors(colors);
        strip->setPixmap(scaleStrip(dark));
    };
    recolor();
    QObject::connect(plot, &rocketplot::PlotWidget::themeChanged, landings, recolor);
    // [/snippet]

    layout->addWidget(plot, 1);
    return page;
}

}  // namespace

DemoPage pointStylesPage()
{
    return {
        .title       = QStringLiteral("Point sizes & colors"),
        .description = QStringLiteral(
            "A scatter series can give every point its own marker size and color, to show a third "
            "and a fourth value. The sizes are diameters in pixels and the colors any QColor: "
            "here steps of one hue, so that stronger wind reads as more."),
        .sourceFile = QStringLiteral("PointStylesPage.cpp"),
        .create     = create,
    };
}

}  // namespace rocketplot::demo
