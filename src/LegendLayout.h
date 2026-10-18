#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <cstddef>
#include <optional>
#include <vector>

#include "rocketplot/enums.h"

class QObject;
class QPainter;

namespace rocketplot
{

namespace core
{
class Occupancy;
}

class Annotation;
class PlotWidget;
class Series;
class TextPainter;
struct PlotLayout;

/// One entry of the legend as drawn: of a series, of the series of a legend group, or of an
/// annotation.
struct LegendEntry
{
    /// The series it stands for: one, or every series of its legend group.
    std::vector<Series*> series;
    /// The series it looks like and is named after: the first named one.
    Series* sample = nullptr;
    /// Or the annotation it stands for.
    Annotation* annotation = nullptr;
    QString     group;  ///< The series' legend group; empty for none
    QString     name;
    QString     value;  ///< The value at the crosshair; empty for none
    QRectF      row;    ///< The whole row, for pointing and clicking

    /// What tells the entry apart from frame to frame: its sample, or its annotation.
    [[nodiscard]] QObject* key() const;
    /// Whether what it stands for shows: any of its series, or its annotation.
    [[nodiscard]] bool isVisible() const;
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
    /// The entry of @p key (LegendEntry::key()), if there is one.
    [[nodiscard]] const LegendEntry* entryOf(const QObject* key) const;
};

/// What the legend shows in a frame besides the series' names.
struct LegendState
{
    /// Each entry shows its series' value at this x (the crosshair's).
    std::optional<double> crosshairX;
    /// The data point the crosshair is on, if it is on one: its series' entry shows the y of
    /// that very point (also when the series isn't sorted by x, and so has no value at an x).
    const Series* crosshairSeries = nullptr;
    std::size_t   crosshairIndex  = 0;
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

/// Draws @p legend; the entry of @p pointed (the key of the entry the user points at) is
/// highlighted.
void drawLegend(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                const LegendLayout& legend, TextPainter& text, const QObject* pointed);

/// The box of a legend of @p size at @p anchor (not BEST) inside @p plotArea; CUSTOM puts it at
/// @p position (see Legend::position()).
[[nodiscard]] QRectF legendRect(const QRectF& plotArea, QSizeF size, LegendAnchor anchor,
                                QPointF position = {});
/// The Legend::position() that puts a legend of @p size with its top-left corner at @p topLeft.
[[nodiscard]] QPointF legendPosition(const QRectF& plotArea, QSizeF size, QPointF topLeft);

}  // namespace rocketplot
