#pragma once

#include <QFont>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <vector>

#include "core/AxisMapping.h"
#include "core/TickGenerator.h"

namespace rocketplot
{

class PlotWidget;
class Series;

/// One axis's share of a layout: how values map to pixels, its ticks and their labels.
struct AxisLayout
{
    core::AxisMapping mapping;
    core::Ticks       ticks;
    QStringList       labels;  ///< One per major tick
};

struct LegendEntry
{
    const Series* series = nullptr;
    QString       name;
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
    QRectF xAxisArea;  ///< Below the plot: x ticks and their labels
    QRectF yAxisArea;  ///< Left of the plot: y ticks and their labels
    QRectF xLabel;
    QRectF yLabel;
    QRectF legend;  ///< Empty when the legend isn't shown

    AxisLayout x;
    AxisLayout y;

    QFont tickFont;
    QFont labelFont;
    QFont titleFont;
    QFont legendFont;

    std::vector<LegendEntry> legendEntries;  ///< Empty when the legend isn't shown
    double                   legendRowHeight = 0.0;
};

/// Lays out @p plot in @p bounds, using @p font as the base font.
[[nodiscard]] PlotLayout layoutPlot(const PlotWidget& plot, const QRectF& bounds, const QFont& font,
                                    double devicePixelRatio);

// Legend geometry shared by layout and drawing.
inline constexpr double kLegendPadding   = 8.0;
inline constexpr double kLegendSwatch    = 24.0;
inline constexpr double kLegendSwatchGap = 8.0;
inline constexpr double kLegendRowGap    = 4.0;
inline constexpr double kTickLabelGap    = 4.0;

}  // namespace rocketplot
