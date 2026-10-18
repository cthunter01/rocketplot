#include "FlagLayout.h"

#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QSizeF>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "PlotLayout.h"
#include "TextPainter.h"
#include "core/LabelStagger.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/PlotWidget.h"

namespace rocketplot
{

namespace
{

// A flag's padding around its content, the space between its icon and its text, between flags in a
// row, and between rows.
constexpr double kPaddingX = 5.0;
constexpr double kPaddingY = 1.0;
constexpr double kIconGap  = 3.0;
constexpr double kGap      = 4.0;
constexpr double kRowGap   = 2.0;

// The flags of @p plot's events as if horizontal: their lengths, and where their icons and text go
// along them, all as thick as the thickest with their content centered across it.
std::vector<EventFlag> measureFlags(const PlotWidget& plot, const PlotLayout& layout,
                                    TextPainter& text)
{
    const QFont&           font      = layout.annotationFont;
    const double           side      = QFontMetricsF(font).height();  // of an icon
    double                 thickness = 0.0;
    std::vector<EventFlag> flags;
    for (const Annotation* annotation : plot.annotations())
    {
        const auto* event = qobject_cast<const EventMarker*>(annotation);
        if (event == nullptr || !event->isVisible() ||
            (event->label().isEmpty() && event->icon().isNull()))
        {
            continue;
        }
        EventFlag flag;
        flag.event = event;
        flag.line  = layout.mappingsFor(*event).x.toPixel(event->x());
        if (!std::isfinite(flag.line))
        {
            continue;
        }
        double along  = kPaddingX;
        double height = 0.0;
        if (!event->icon().isNull())
        {
            flag.icon = QRectF(along, 0.0, side, side);
            along += side + (event->label().isEmpty() ? 0.0 : kIconGap);
            height = side;
        }
        if (!event->label().isEmpty())
        {
            const QSizeF size = text.size(event->label(), font);
            flag.text         = QRectF(along, 0.0, size.width(), size.height());
            along += size.width();
            height = std::max(height, size.height());
        }
        flag.size = QSizeF(along + kPaddingX, 0.0);
        thickness = std::max(thickness, height + (2.0 * kPaddingY));
        flags.push_back(flag);
    }
    for (EventFlag& flag : flags)
    {
        flag.size.setHeight(thickness);
        if (!flag.icon.isNull())
        {
            flag.icon.moveTop((thickness - side) / 2.0);
        }
        if (!flag.text.isNull())
        {
            flag.text = QRectF(flag.text.left(), 0.0, flag.text.width(), thickness);
        }
    }
    return flags;
}

// The left edge of a flag @p width wide on the line at @p line: right of it, or left of it where
// it would leave the plot area, flush with the pixel column the line is drawn in.
double flagLeft(const PlotLayout& layout, double line, double width)
{
    const QRectF& area   = layout.plot;
    const double  dpr    = layout.devicePixelRatio;
    const double  column = std::floor(line * dpr) / dpr;
    const bool    flipped =
        line <= area.right() && column + width > area.right() && column - width >= area.left();
    return flipped ? column + (1.0 / dpr) - width : column;
}

// Vertical flags: one row, reading upward; one too long is shortened and its text elided.
void placeVertical(FlagLayout& result, std::vector<EventFlag> flags, const PlotLayout& layout,
                   double top, double maxHeight)
{
    const double thickness = flags.front().size.height();
    const double maxLength = std::max(thickness, maxHeight);
    for (EventFlag& flag : flags)
    {
        if (flag.size.width() > maxLength)
        {
            flag.size.setWidth(maxLength);
            if (!flag.text.isNull())
            {
                flag.text.setRight(std::max(flag.text.left(), maxLength - kPaddingX));
            }
        }
        flag.box =
            QRectF(flagLeft(layout, flag.line, thickness), top, thickness, flag.size.width());
        result.height = std::max(result.height, flag.size.width());
        result.flags.push_back(flag);
    }
}

// Horizontal flags: a flag that would cover one before it goes into the first row with room.
void placeInRows(FlagLayout& result, const std::vector<EventFlag>& flags, const PlotLayout& layout,
                 double top, double maxHeight)
{
    const double pitch   = flags.front().size.height() + kRowGap;
    const int    maxRows = std::max(1, static_cast<int>((maxHeight + kRowGap) / pitch));
    std::vector<core::LabelSpan> spans;
    spans.reserve(flags.size());
    for (const EventFlag& flag : flags)
    {
        const double left = flagLeft(layout, flag.line, flag.size.width());
        spans.push_back({.left = left, .right = left + flag.size.width()});
    }
    const std::vector<int> rows = core::staggerLabels(spans, kGap, maxRows);
    int                    used = 0;
    for (std::size_t i = 0; i < flags.size(); ++i)
    {
        if (rows[i] == core::kNoRow)
        {
            continue;
        }
        EventFlag flag = flags[i];
        flag.box =
            QRectF(spans[i].left, top + (rows[i] * pitch), flag.size.width(), flag.size.height());
        used = std::max(used, rows[i] + 1);
        result.flags.push_back(flag);
    }
    result.height = (used * pitch) - kRowGap;
}

}  // namespace

FlagLayout layoutEventFlags(const PlotWidget& plot, const PlotLayout& layout, TextPainter& text,
                            double top, double maxHeight, Qt::Orientation orientation)
{
    FlagLayout result;
    result.vertical              = orientation == Qt::Vertical;
    std::vector<EventFlag> flags = measureFlags(plot, layout, text);
    if (flags.empty())
    {
        return result;
    }
    if (result.vertical)
    {
        placeVertical(result, std::move(flags), layout, top, maxHeight);
    }
    else
    {
        placeInRows(result, flags, layout, top, maxHeight);
    }
    return result;
}

}  // namespace rocketplot
