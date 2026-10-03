#include "rocketplot/Theme.h"

#include <QColor>
#include <QSignalSpy>
#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::Theme;
using rocketplot::ThemeMode;

// The WCAG contrast ratio between two colors, from 1 to 21.
double contrast(const QColor& a, const QColor& b)
{
    const auto luminance = [](const QColor& color) {
        const auto linear = [](float channel) {
            const auto value = static_cast<double>(channel);
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return (0.2126 * linear(color.redF())) + (0.7152 * linear(color.greenF())) +
               (0.0722 * linear(color.blueF()));
    };
    const double first  = luminance(a);
    const double second = luminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

// The contrast against the background of the series color that has the least.
double faintestSeries(const Theme& theme)
{
    double faintest = 21.0;
    for (const QColor& color : theme.seriesColors)
    {
        faintest = std::min(faintest, contrast(color, theme.background));
    }
    return faintest;
}

TEST(Theme, SeriesColorsRepeat)
{
    const Theme theme = Theme::light();
    ASSERT_EQ(theme.seriesColors.size(), 8);
    EXPECT_EQ(theme.seriesColor(0), theme.seriesColors.front());
    EXPECT_EQ(theme.seriesColor(8), theme.seriesColors.front());
    Theme none;
    none.text = Qt::red;
    EXPECT_EQ(none.seriesColor(3), QColor(Qt::red));  // without series colors: the text color
}

TEST(Theme, ThePresetsDiffer)
{
    EXPECT_NE(Theme::light(), Theme::dark());
    EXPECT_NE(Theme::highContrast(), Theme::dark());
    EXPECT_NE(Theme::print(), Theme::light());
    EXPECT_EQ(Theme::light(), Theme::light());
}

TEST(Theme, HighContrastIsWhiteOnBlackWithNothingFaint)
{
    const Theme theme = Theme::highContrast();
    EXPECT_EQ(theme.background, QColor(Qt::black));
    EXPECT_EQ(theme.text, QColor(Qt::white));
    EXPECT_EQ(theme.legendBackground.alpha(), 255);
    EXPECT_GT(theme.lineWidth, Theme::dark().lineWidth);
    EXPECT_GT(contrast(theme.gridLine, theme.background),
              contrast(Theme::dark().gridLine, Theme::dark().background));
    EXPECT_GE(faintestSeries(theme), 3.0);
}

TEST(Theme, PrintIsBlackOnWhiteWithColorsThatShowOnPaper)
{
    const Theme theme = Theme::print();
    EXPECT_EQ(theme.background, QColor(Qt::white));
    EXPECT_EQ(theme.text, QColor(Qt::black));
    EXPECT_EQ(theme.seriesColors.size(), Theme::light().seriesColors.size());
    // Every series color shows on white; some of light()'s are too faint there.
    EXPECT_GE(faintestSeries(theme), 3.0);
    EXPECT_LT(faintestSeries(Theme::light()), 3.0);
    EXPECT_GT(contrast(theme.axisLine, theme.background),
              contrast(Theme::light().axisLine, Theme::light().background));
}

TEST(Theme, AModeForEachPreset)
{
    PlotWidget       plot;
    const QSignalSpy changed(&plot, &PlotWidget::themeChanged);
    plot.setThemeMode(ThemeMode::HIGH_CONTRAST);
    EXPECT_EQ(plot.theme(), Theme::highContrast());
    plot.setThemeMode(ThemeMode::PRINT);
    EXPECT_EQ(plot.theme(), Theme::print());
    plot.setThemeMode(ThemeMode::DARK);
    EXPECT_EQ(plot.theme(), Theme::dark());
    EXPECT_EQ(changed.count(), 3);
    plot.setTheme(Theme::print());
    EXPECT_EQ(plot.themeMode(), ThemeMode::CUSTOM);
}

}  // namespace
