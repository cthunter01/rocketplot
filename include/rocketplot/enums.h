#pragma once

#include <QObject>
#include <cstdint>

#include "rocketplot/export.h"

/// Enumerations shared by the plot classes, registered with Qt's meta-object system (Q_ENUM_NS) so
/// they work in properties, QVariant and debug output.
namespace rocketplot
{
Q_NAMESPACE_EXPORT(ROCKETPLOT_EXPORT)

/// Shape drawn at each data point.
enum class Marker : std::uint8_t
{
    NONE,
    CIRCLE,
    SQUARE,
    DIAMOND,
    TRIANGLE,
    CROSS,
    PLUS,
};
Q_ENUM_NS(Marker)

/// How a series shows the errors of its points (Series::setYErrors(), Series::setXErrors()).
enum class ErrorStyle : std::uint8_t
{
    BARS,  ///< A whisker with caps through each point (the default for scatter series). Where the
           ///< points of a series sorted by x are too close together to tell their bars apart, the
           ///< y errors are drawn as a band instead
    BAND,  ///< y errors as a shaded band around the series (the default for lines); x errors still
           ///< as bars. Needs x sorted: otherwise bars are drawn
};
Q_ENUM_NS(ErrorStyle)

/// Whether an annotation is drawn under or over the series.
enum class AnnotationLayer : std::uint8_t
{
    BELOW_SERIES,  ///< Over the grid, under the data (the default for shaded spans)
    ABOVE_SERIES,  ///< Over the data (the default for lines, text and event markers)
};
Q_ENUM_NS(AnnotationLayer)

/// Where the legend sits inside the plot area.
enum class LegendAnchor : std::uint8_t
{
    BEST,  ///< The corner or edge where it covers the least data, moving only when another is
           ///< clearly better (the default)
    TOP_LEFT,
    TOP,
    TOP_RIGHT,
    RIGHT,
    BOTTOM_RIGHT,
    BOTTOM,
    BOTTOM_LEFT,
    LEFT,
    CUSTOM,  ///< At Legend::position(), e.g. where the user dragged it
};
Q_ENUM_NS(LegendAnchor)

/// Where a plot's colors come from.
enum class ThemeMode : std::uint8_t
{
    SYSTEM,  ///< Light or dark to match the widget's palette, following changes (the default)
    LIGHT,   ///< Theme::light()
    DARK,    ///< Theme::dark()
    HIGH_CONTRAST,  ///< Theme::highContrast()
    PRINT,          ///< Theme::print()
    CUSTOM,         ///< The theme given to PlotWidget::setTheme()
};
Q_ENUM_NS(ThemeMode)

/// How an axis spaces and labels its values.
enum class ScaleType : std::uint8_t
{
    LINEAR,
    LOGARITHMIC,  ///< Each power of ten takes the same length; values <= 0 aren't shown
    DATE_TIME,    ///< Values are seconds since 1970-01-01 00:00 UTC; labels are dates and times
};
Q_ENUM_NS(ScaleType)

/// How an axis writes its numbers.
enum class NumberFormat : std::uint8_t
{
    AUTO,   ///< Plain, with a common offset (+1.7×10⁹) and a ×10ⁿ multiplier when labels would be
            ///< long
    SI,     ///< SI prefixes: 250m, 1.5k, 20µ
    PLAIN,  ///< Always the full number
};
Q_ENUM_NS(NumberFormat)

/// What an axis shows while autoscale is on.
enum class AutoscaleMode : std::uint8_t
{
    FIT_ALL,        ///< All the data of the visible series
    FIT_VISIBLE,    ///< y axes: the data inside the current x range (like FIT_ALL on an x axis)
    FOLLOW_LATEST,  ///< x axes: the newest Axis::followWindow() of data, scrolling as data arrives
};
Q_ENUM_NS(AutoscaleMode)

/// Which plots of a PlotGrid share their x axis (through a PlotLink).
enum class GridLink : std::uint8_t
{
    NONE,     ///< None: each plot has its own
    COLUMNS,  ///< The plots of each column (the default)
    ALL,      ///< All the plots
};
Q_ENUM_NS(GridLink)

/// A pointer gesture that InputBindings maps to an action.
enum class Gesture : std::uint8_t
{
    DRAG,   ///< Press a button and move (a one-finger drag on a touchscreen is the left button)
    CLICK,  ///< Press and release a button without moving
    DOUBLE_CLICK,  ///< Also a double tap on a touchscreen (the left button)
    WHEEL,         ///< A mouse wheel
    SCROLL,        ///< Two-finger scrolling on a trackpad (a Magic Mouse's surface too)
    PINCH,         ///< Two fingers on a trackpad or touchscreen moving apart or together
};
Q_ENUM_NS(Gesture)

/// What a gesture does to the view. Over the plot it applies to every axis (or the ones it names);
/// over an axis, to that axis alone.
enum class PlotAction : std::uint8_t
{
    NONE,
    PAN,       ///< DRAG, SCROLL: move the view with the pointer
    ZOOM,      ///< WHEEL, SCROLL, PINCH: zoom about the pointer
    ZOOM_X,    ///< WHEEL, SCROLL, PINCH: zoom the x axis only
    ZOOM_Y,    ///< WHEEL, SCROLL, PINCH: zoom the y axes only
    BOX_ZOOM,  ///< DRAG: zoom to the rectangle dragged out (a thin one zooms one axis)
    RESET,     ///< CLICK, DOUBLE_CLICK: back to autoscale (PlotWidget::resetView())
    BACK,      ///< CLICK, DOUBLE_CLICK: the previous view (PlotWidget::back())
    FORWARD,   ///< CLICK, DOUBLE_CLICK: the next view (PlotWidget::forward())
};
Q_ENUM_NS(PlotAction)

}  // namespace rocketplot
