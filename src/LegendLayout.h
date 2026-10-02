#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <optional>
#include <vector>

#include "rocketplot/enums.h"

class QPainter;

namespace rocketplot
{

namespace core
{
class Occupancy;
}

class PlotWidget;
class Series;
class TextPainter;
struct PlotLayout;

/// One entry of the legend as drawn.
struct LegendEntry
{
    Series* series = nullptr;
    QString name;
    QString value;  ///< The series' value at the crosshair; empty for none
    QRectF  row;    ///< The whole row, for pointing and clicking
};

/// Where the legend goes in one frame, and what it says. The legend is drawn over the plot's
/// cached rendering, so it can move and show values at the crosshair without redrawing the data.
struct LegendLayout
{
    QRectF                   box;  ///< Empty when the legend isn't shown
    std::vector<LegendEntry> entries;
    double                   rowHeight  = 0.0;
    double                   valueWidth = 0.0;  ///< Of the value column; 0 without one
    LegendAnchor             anchor     = LegendAnchor::TOP_RIGHT;  ///< Where it went (not BEST)

    [[nodiscard]] bool isShown() const noexcept { return !box.isEmpty(); }
    /// The entry whose row contains @p position, if any.
    [[nodiscard]] const LegendEntry* entryAt(QPointF position) const;
};

/// What the legend shows in a frame besides the series' names.
struct LegendState
{
    /// Each entry shows its series' value at this x (the crosshair's).
    std::optional<double> crosshairX;
    /// The value column is at least this wide, so it doesn't shrink and grow as the crosshair
    /// moves.
    double minValueWidth = 0.0;
    /// Where the data is drawn, for a BEST legend.
    const core::Occupancy* occupancy = nullptr;
    /// Where a BEST legend went last time: it stays there unless another spot is clearly better.
    std::optional<LegendAnchor> previousBest;
};

/// Lays out the legend of @p plot over its layout.
[[nodiscard]] LegendLayout layoutLegend(const PlotWidget& plot, const PlotLayout& layout,
                                        TextPainter& text, const LegendState& state);

/// Draws @p legend; the entry of @p pointed (the series the user points at) is highlighted.
void drawLegend(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                const LegendLayout& legend, TextPainter& text, const Series* pointed);

/// The box of a legend of @p size at @p anchor (not BEST) inside @p plotArea; CUSTOM puts it at
/// @p position (see Legend::position()).
[[nodiscard]] QRectF legendRect(const QRectF& plotArea, QSizeF size, LegendAnchor anchor,
                                QPointF position = {});
/// The Legend::position() that puts a legend of @p size with its top-left corner at @p topLeft.
[[nodiscard]] QPointF legendPosition(const QRectF& plotArea, QSizeF size, QPointF topLeft);

}  // namespace rocketplot
