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

}  // namespace rocketplot
