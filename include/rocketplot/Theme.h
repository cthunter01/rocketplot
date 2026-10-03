#pragma once

#include <QColor>
#include <QList>

#include "rocketplot/export.h"

namespace rocketplot
{

/// The colors and sizes of everything a plot draws apart from each series's own style. Sizes are in
/// device-independent pixels; font sizes are factors of the widget's font.
///
/// The built-in themes use a categorical palette whose order was checked for color-vision
/// deficiencies, with separate steps for light and dark backgrounds, and recessive hairline axes
/// and grid so the data stands out.
struct ROCKETPLOT_EXPORT Theme
{
    QColor background;        ///< Behind everything
    QColor text;              ///< Title and legend text
    QColor secondaryText;     ///< Axis labels and tick labels
    QColor axisLine;          ///< Axis lines and ticks
    QColor gridLine;          ///< Grid lines at the major ticks
    QColor minorGridLine;     ///< Grid lines at the minor ticks (when shown)
    QColor legendBackground;  ///< Fill of the legend box (usually translucent)
    QColor legendBorder;      ///< Hairline around the legend box
    QColor crosshair;         ///< Crosshair lines
    QColor tagBackground;     ///< The crosshair's value tags on the axes
    QColor tagText;
    QColor zoomBoxBorder;  ///< The box dragged out to zoom
    QColor zoomBoxFill;
    QColor annotation;  ///< Reference lines, shaded spans and event markers without a color of
                        ///< their own (text annotations use the text color)

    /// Series colors, given out in this order. A plot with more series than colors starts over.
    QList<QColor> seriesColors;

    double lineWidth  = 2.0;  ///< Default width of series lines
    double markerSize = 8.0;  ///< Default marker diameter
    double markerRingWidth =
        2.0;  ///< Background-colored ring around markers, so overlapping ones stay apart
    double errorBarWidth       = 1.5;   ///< Line width of error bars
    double errorCapSize        = 6.0;   ///< Width of the caps at the ends of error bars
    double bandOpacity         = 0.15;  ///< Of error bands, which are filled in the series color
    double annotationLineWidth = 1.0;   ///< Reference lines, event markers and arrows
    double spanOpacity         = 0.12;  ///< Of shaded spans, which are filled in their color
    double tickLength          = 5.0;
    double minorTickLength     = 3.0;
    double tickFontScale       = 0.9;
    double annotationFontScale = 0.9;  ///< Annotation text and labels
    double labelFontScale      = 1.0;
    double titleFontScale      = 1.2;

    /// The color for the series that was given color index @p index.
    [[nodiscard]] QColor seriesColor(qsizetype index) const;

    [[nodiscard]] static Theme light();
    [[nodiscard]] static Theme dark();

    friend bool operator==(const Theme&, const Theme&) = default;
};

}  // namespace rocketplot
