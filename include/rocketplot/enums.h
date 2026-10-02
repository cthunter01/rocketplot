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

/// Where the legend sits inside the plot area.
enum class LegendAnchor : std::uint8_t
{
    TOP_LEFT,
    TOP,
    TOP_RIGHT,
    RIGHT,
    BOTTOM_RIGHT,
    BOTTOM,
    BOTTOM_LEFT,
    LEFT,
};
Q_ENUM_NS(LegendAnchor)

/// Where a plot's colors come from.
enum class ThemeMode : std::uint8_t
{
    SYSTEM,  ///< Light or dark to match the widget's palette, following changes (the default)
    LIGHT,   ///< Theme::light()
    DARK,    ///< Theme::dark()
    CUSTOM,  ///< The theme given to PlotWidget::setTheme()
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

}  // namespace rocketplot
