#pragma once

#include <QFont>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <vector>

#include "core/AxisMapping.h"

namespace rocketplot
{

class Axis;
class PlotWidget;
class Series;
class TextPainter;

/// One axis's share of a layout.
struct AxisLayout
{
    bool                shown = false;
    core::AxisMapping   mapping;
    std::vector<double> major;
    std::vector<double> minor;
    QStringList         labels;      ///< One per major tick
    QString             annotation;  ///< Offset, multiplier or date context, shown once
    QRectF              area;        ///< Ticks and tick labels
    QRectF              titleRect;   ///< The axis label
    QRectF              annotationRect;
};

/// Smallest margins either side of the plot area (linked plots line up with these).
struct LayoutConstraints
{
    double minLeft  = 0.0;
    double minRight = 0.0;
};

/// Where everything goes for one frame. Computed from the plot's state and the target rectangle, so
/// the same code lays out the widget and (later) image and vector exports.
struct PlotLayout
{
    bool   valid            = false;  ///< False when the target is too small to plot in
    double devicePixelRatio = 1.0;

    QRectF bounds;  ///< The whole target
    QRectF plot;    ///< Inside the axes: where data is drawn
    QRectF title;

    AxisLayout x;
    AxisLayout y;
    AxisLayout y2;  ///< The secondary y axis, on the right

    QFont tickFont;
    QFont labelFont;
    QFont titleFont;
    QFont legendFont;

    /// The margins left and right of the plot area that this plot's content needs, before
    /// constraints.
    double naturalLeft  = 0.0;
    double naturalRight = 0.0;

    /// The y layout a series is drawn against.
    [[nodiscard]] const AxisLayout& yFor(const Series& series) const;
};

/// Lays out @p plot in @p bounds, using @p font as the base font.
[[nodiscard]] PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                                    double devicePixelRatio, TextPainter& text,
                                    LayoutConstraints constraints = {});

/// The scale an axis maps values with.
[[nodiscard]] core::Scale scaleOf(const Axis& axis);

/// The value at @p pixel of an axis laid out as @p mapping, written in full to the precision a
/// pixel resolves there (for the crosshair's tags).
[[nodiscard]] QString readoutLabel(const Axis& axis, const core::AxisMapping& mapping,
                                   double pixel);
/// A data @p value of an axis laid out as @p mapping, written to the precision a pixel resolves
/// where it is (for the legend's values).
[[nodiscard]] QString valueLabel(const Axis& axis, const core::AxisMapping& mapping, double value);

// Geometry shared by layout and drawing.
inline constexpr double kTickLabelGap = 4.0;

}  // namespace rocketplot
