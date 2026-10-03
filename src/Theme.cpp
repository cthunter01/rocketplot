#include "rocketplot/Theme.h"

#include <QColor>
#include <QList>

namespace rocketplot
{

namespace
{

// Alpha of the legend box over the plot (out of 255).
constexpr int kLegendAlpha = 230;
// Hairline borders: the text color at 10%.
constexpr int kBorderAlpha = 26;
// The crosshair: the secondary text color at 60%. The zoom box: the first series color, filled at
// 12%. Annotations: a gray midway between the background and the text, quieter than the data but
// 5:1 against the background, and against the background-colored text on an event's flag.
constexpr int kCrosshairAlpha = 153;
constexpr int kZoomFillAlpha  = 31;

}  // namespace

QColor Theme::seriesColor(qsizetype index) const
{
    if (seriesColors.isEmpty())
    {
        return text;
    }
    return seriesColors.at(index % seriesColors.size());
}

Theme Theme::light()
{
    Theme theme;
    theme.background       = QColor(0xfc, 0xfc, 0xfb);
    theme.text             = QColor(0x0b, 0x0b, 0x0b);
    theme.secondaryText    = QColor(0x52, 0x51, 0x4e);
    theme.axisLine         = QColor(0xc3, 0xc2, 0xb7);
    theme.gridLine         = QColor(0xe1, 0xe0, 0xd9);
    theme.minorGridLine    = QColor(0xf0, 0xef, 0xeb);
    theme.legendBackground = QColor(0xfc, 0xfc, 0xfb, kLegendAlpha);
    theme.legendBorder     = QColor(0x0b, 0x0b, 0x0b, kBorderAlpha);
    theme.crosshair        = QColor(0x52, 0x51, 0x4e, kCrosshairAlpha);
    theme.tagBackground    = QColor(0x52, 0x51, 0x4e);
    theme.tagText          = QColor(0xfc, 0xfc, 0xfb);
    theme.zoomBoxBorder    = QColor(0x2a, 0x78, 0xd6);
    theme.zoomBoxFill      = QColor(0x2a, 0x78, 0xd6, kZoomFillAlpha);
    theme.annotation       = QColor(0x70, 0x6e, 0x69);
    // Blue, orange, aqua, yellow, magenta, green, violet, red: adjacent colors stay apart under the
    // common color-vision deficiencies. Keep the order.
    theme.seriesColors = {
        QColor(0x2a, 0x78, 0xd6), QColor(0xeb, 0x68, 0x34), QColor(0x1b, 0xaf, 0x7a),
        QColor(0xed, 0xa1, 0x00), QColor(0xe8, 0x7b, 0xa4), QColor(0x00, 0x83, 0x00),
        QColor(0x4a, 0x3a, 0xa7), QColor(0xe3, 0x49, 0x48),
    };
    return theme;
}

Theme Theme::dark()
{
    Theme theme;
    theme.background       = QColor(0x1a, 0x1a, 0x19);
    theme.text             = QColor(0xff, 0xff, 0xff);
    theme.secondaryText    = QColor(0xc3, 0xc2, 0xb7);
    theme.axisLine         = QColor(0x52, 0x51, 0x4e);
    theme.gridLine         = QColor(0x2c, 0x2c, 0x2a);
    theme.minorGridLine    = QColor(0x23, 0x23, 0x22);
    theme.legendBackground = QColor(0x1a, 0x1a, 0x19, kLegendAlpha);
    theme.legendBorder     = QColor(0xff, 0xff, 0xff, kBorderAlpha);
    theme.crosshair        = QColor(0xc3, 0xc2, 0xb7, kCrosshairAlpha);
    theme.tagBackground    = QColor(0xc3, 0xc2, 0xb7);
    theme.tagText          = QColor(0x1a, 0x1a, 0x19);
    theme.zoomBoxBorder    = QColor(0x39, 0x87, 0xe5);
    theme.zoomBoxFill      = QColor(0x39, 0x87, 0xe5, kZoomFillAlpha);
    theme.annotation       = QColor(0x90, 0x8e, 0x87);
    // The same hues as light(), stepped for a dark background.
    theme.seriesColors = {
        QColor(0x39, 0x87, 0xe5), QColor(0xd9, 0x59, 0x26), QColor(0x19, 0x9e, 0x70),
        QColor(0xc9, 0x85, 0x00), QColor(0xd5, 0x51, 0x81), QColor(0x00, 0x83, 0x00),
        QColor(0x90, 0x85, 0xe9), QColor(0xe6, 0x67, 0x67),
    };
    return theme;
}

}  // namespace rocketplot
