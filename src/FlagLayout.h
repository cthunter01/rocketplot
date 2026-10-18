#pragma once

#include <QRectF>
#include <QSizeF>
#include <Qt>
#include <vector>

namespace rocketplot
{

class EventMarker;
class PlotWidget;
class TextPainter;
struct PlotLayout;

/// One event's flag as laid out.
struct EventFlag
{
    const EventMarker* event = nullptr;
    /// Where it is, in the plot's coordinates (upright, also for a vertical flag).
    QRectF box;
    /// The flag as if it were horizontal: its length along the text and its thickness. A vertical
    /// flag is this turned to read upward, its start at the bottom.
    QSizeF size;
    /// Where its icon and its text go in that frame, from its top-left corner; null for none.
    QRectF icon;
    QRectF text;
    /// The x of its event's line (in pixels).
    double line = 0.0;
};

/// Where the flags of a plot's events go in one layout.
struct FlagLayout
{
    /// The flags that have room, in the order they are drawn. Flags of events out of view count
    /// too (their boxes are outside the plot area), so that panning doesn't move flags between
    /// rows.
    std::vector<EventFlag> flags;
    /// How far below the top they start reach (the rows used, or the longest vertical flag); 0
    /// without flags.
    double height = 0.0;
    /// Flags turned to read upward, in one row.
    bool vertical = false;
};

/// The flags of @p plot's events (those with a label or an icon), from @p top down: horizontal
/// flags staggered into rows so that they don't cover each other, vertical ones in one row. They
/// take no more than @p maxHeight; horizontal flags without room are left out, vertical ones
/// shortened (their text elided). @p layout's plot area and x mapping must be set.
[[nodiscard]] FlagLayout layoutEventFlags(const PlotWidget& plot, const PlotLayout& layout,
                                          TextPainter& text, double top, double maxHeight,
                                          Qt::Orientation orientation);

/// From the top of the plot area to the first row of flags over the data.
inline constexpr double kFlagTop = 2.0;

}  // namespace rocketplot
