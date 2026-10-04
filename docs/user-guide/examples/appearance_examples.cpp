// Examples of appearance.md.

#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QString>
#include <QWidget>
#include <Qt>
#include <array>
#include <cstddef>
#include <vector>

#include "Examples.h"
#include "SampleData.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot::guide
{

namespace
{

void chillDown(PlotWidget* plot)
{
    const std::vector<double> time = linspace(0.0, 600.0, 601);
    plot->addLine(time, settling(time, 18.0, -160.0, 120.0, 1.2, 1), "LOX feed line");
    plot->addLine(time, settling(time, 21.0, -95.0, 200.0, 0.9, 2), "Turbopump housing");
    plot->addLine(time, settling(time, 20.0, -40.0, 300.0, 0.7, 3), "Tank wall");
    plot->xAxis()->setLabel("Time (s)");
    plot->yAxis()->setLabel("Temperature (°C)");
}

// The same plot in each of the built-in themes.
QWidget* builtInThemes(QWidget* parent)
{
    auto* window = new QWidget(parent);
    auto* layout = new QGridLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    struct Entry
    {
        ThemeMode   mode;
        const char* title;
    };
    constexpr std::array kThemes{
        Entry{.mode = ThemeMode::LIGHT, .title = "LIGHT"},
        Entry{.mode = ThemeMode::DARK, .title = "DARK"},
        Entry{.mode = ThemeMode::HIGH_CONTRAST, .title = "HIGH_CONTRAST"},
        Entry{.mode = ThemeMode::PRINT, .title = "PRINT"},
    };
    const std::vector<double> time = linspace(0.0, 600.0, 601);
    for (std::size_t i = 0; i < kThemes.size(); ++i)
    {
        // Warming up again: curves that leave a corner free for the legend in a small plot.
        auto* plot = new PlotWidget(window);
        plot->addLine(time, settling(time, -160.0, 18.0, 120.0, 1.2, 1), "LOX feed line");
        plot->addLine(time, settling(time, -95.0, 21.0, 200.0, 0.9, 2), "Turbopump housing");
        plot->addLine(time, settling(time, -40.0, 20.0, 300.0, 0.7, 3), "Tank wall");
        plot->xAxis()->setLabel("Time (s)");
        plot->yAxis()->setLabel("Temperature (°C)");
        plot->setTitle(kThemes.at(i).title);
        plot->setThemeMode(kThemes.at(i).mode);
        layout->addWidget(plot, static_cast<int>(i / 2), static_cast<int>(i % 2));
    }
    return window;
}

void customTheme(PlotWidget* plot)
{
    chillDown(plot);
    plot->setTitle("Engine chill-down");
    // [appearance-custom]
    rocketplot::Theme theme = rocketplot::Theme::light();  // start from a built-in theme
    theme.background        = QColor("#fbf6ea");
    theme.gridLine          = QColor("#e6dcc6");
    theme.axisLine          = QColor("#8a7f6a");
    theme.lineWidth         = 2.5;  // of every line that wasn't given a width of its own
    theme.titleFontScale    = 1.5;  // times the widget's font
    plot->setTheme(theme);          // the mode is now ThemeMode::CUSTOM
    // [/appearance-custom]
}

void richText(PlotWidget* plot)
{
    const std::vector<double> time = linspace(0.0, 10.0, 401);
    std::vector<double>       speed;
    speed.reserve(time.size());
    for (const double t : time)
    {
        speed.push_back(0.5 * 9.81 * t * t / 10.0);
    }
    plot->addLine(time, speed);
    // [appearance-rich-text]
    plot->setTitle("Descent rate <i>v<sub>z</sub></i>");
    plot->xAxis()->setLabel("Time since release, <i>t</i> − <i>t</i><sub>0</sub> (s)");
    plot->yAxis()->setLabel("<i>v<sub>z</sub></i> (m s<sup>−1</sup>)");
    plot->addHorizontalLine(40.0, "<b>Limit</b>: 40 m s<sup>−1</sup>")
        ->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);  // above the line's left end
    // [/appearance-rich-text]
}

void settings(const QString& /*scratch*/)
{
    PlotWidget  window;
    PlotWidget* plot = &window;
    // [appearance-mode]
    plot->setThemeMode(rocketplot::ThemeMode::DARK);    // always dark
    plot->setThemeMode(rocketplot::ThemeMode::SYSTEM);  // the default: follows the desktop
    // [/appearance-mode]

    // [appearance-font]
    QFont font = plot->font();
    font.setPointSizeF(11.0);  // or setFamily(), setWeight(), ...
    plot->setFont(font);       // tick labels, axis labels, the title and the legend all follow
    // [/appearance-font]

    // [appearance-read]
    // The colors the plot draws with now, for things drawn beside it.
    const rocketplot::Theme& current    = plot->theme();
    const QColor             background = current.background;
    const QColor             firstColor = current.seriesColor(0);
    // [/appearance-read]
    static_cast<void>(background);
    static_cast<void>(firstColor);
}

}  // namespace

void addAppearanceExamples(Examples& examples)
{
    examples.figures.push_back(
        {.name = QStringLiteral("appearance-themes"), .make = builtInThemes, .size = {720, 560}});
    examples.addPlot(QStringLiteral("appearance-custom"), customTheme);
    examples.addPlot(QStringLiteral("appearance-rich-text"), richText);
    examples.demonstrations.push_back(
        {.name = QStringLiteral("appearance settings"), .run = settings});
}

}  // namespace rocketplot::guide
