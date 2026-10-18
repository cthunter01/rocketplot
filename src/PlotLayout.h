#pragma once

#include <QFont>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <optional>
#include <vector>

#include "FlagLayout.h"
#include "core/AxisMapping.h"
#include "core/LegendFlow.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

class Annotation;
class Axis;
class PlotWidget;
class Series;
class TextPainter;

/// One axis's share of a layout.
struct AxisLayout
{
    bool                shown   = false;
    bool                labeled = true;  ///< Whether the tick labels and the annotation are drawn
    core::AxisMapping   mapping;
    std::vector<double> major;
    std::vector<double> minor;
    QStringList         labels;      ///< One per major tick
    QString             annotation;  ///< Offset, multiplier or date context, shown once
    QRectF              area;        ///< Ticks and tick labels
    QRectF              titleRect;   ///< The axis label
    QRectF              annotationRect;
};

/// Smallest margins around the plot area: linked plots line up with these left and right, and
/// the plots of a grid on every side.
struct LayoutConstraints
{
    double minLeft   = 0.0;
    double minRight  = 0.0;
    double minTop    = 0.0;
    double minBottom = 0.0;

    [[nodiscard]] bool operator==(const LayoutConstraints&) const = default;
};

/// Ranges to lay a plot out with instead of its axes' own, which stay as they are: an export
/// that needs other ranges than the screen does (headroom for event flags at another height).
struct RangeOverrides
{
    std::optional<Range> x;
    std::optional<Range> y;
    std::optional<Range> y2;
};

/// A legend outside the plot area (LegendAnchor::OUTSIDE_BOTTOM, OUTSIDE_RIGHT), which the plot
/// area makes room for.
struct OutsideLegend
{
    QRectF           box;  ///< Null when there is none
    core::LegendGrid grid;
    std::size_t      entries    = 0;  ///< How many of the legend's entries there is room for
    double           rowHeight  = 0.0;
    double           valueWidth = 0.0;  ///< Kept for the values at the crosshair; 0 for none
};

/// The mappings an annotation's x and y values are drawn with.
struct AnnotationMappings
{
    core::AxisMapping x;
    core::AxisMapping y;
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
    QRectF subtitle;
    /// The band between the title and the plot area that event flags fly in
    /// (FlagPlacement::ABOVE_PLOT_AREA); null otherwise.
    QRectF flagBand;
    /// The flags of the plot's events: over the plot area, or in the band.
    FlagLayout flags;
    /// The legend, when it is outside the plot area.
    OutsideLegend legend;

    AxisLayout x;
    AxisLayout y;
    AxisLayout y2;  ///< The secondary y axis, on the right
    /// The key of the color scale, right of the plot area (and of y2): its bar of colors, as tall
    /// as the plot area, and its ticks, labels and title beside it.
    bool       colorKeyShown = false;
    AxisLayout colorKey;
    QRectF     colorBar;

    QFont tickFont;
    QFont labelFont;
    QFont titleFont;
    QFont subtitleFont;
    QFont legendFont;
    QFont annotationFont;

    /// The margins around the plot area that this plot's content needs, before constraints.
    double naturalLeft   = 0.0;
    double naturalRight  = 0.0;
    double naturalTop    = 0.0;
    double naturalBottom = 0.0;

    /// The y layout a series is drawn against, or an annotation's y values are on.
    [[nodiscard]] const AxisLayout& yFor(const Series& series) const;
    [[nodiscard]] const AxisLayout& yFor(const Annotation& annotation) const;
    /// How @p annotation's values map to pixels: through the x axis and its y axis, or for one
    /// pinned to the plot area, as fractions of the plot area (y upward).
    [[nodiscard]] AnnotationMappings mappingsFor(const Annotation& annotation) const;
};

/// Lays out @p plot in @p bounds, using @p font as the base font, with @p overrides of its axes'
/// ranges (if any).
[[nodiscard]] PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                                    double devicePixelRatio, TextPainter& text,
                                    LayoutConstraints     constraints = {},
                                    const RangeOverrides& overrides   = {});

/// Whether a series of @p plot is on its secondary y axis (which shows it then).
[[nodiscard]] bool hasSeriesOnSecondary(const PlotWidget& plot);

/// @p base at @p scale times its size (a theme's font scales).
[[nodiscard]] QFont scaledFont(const QFont& base, double scale);

/// @p font antialiased in gray, as Qt draws text into a pixmap or an image. What a widget draws
/// over its cached rendering (the legend, the crosshair's labels, a grid's title) then looks like
/// the rest and like an export, where fontconfig asks for subpixel antialiasing (as Ubuntu's does).
[[nodiscard]] QFont grayAntialiased(QFont font);

/// The scale an axis maps values with.
[[nodiscard]] core::Scale scaleOf(const Axis& axis);

/// The title beside the color key: the scale's label, and what its tick labels leave out.
[[nodiscard]] QString colorKeyTitle(const QString& label, const QString& annotation);

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
