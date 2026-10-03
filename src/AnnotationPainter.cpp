#include "AnnotationPainter.h"

#include <QColor>
#include <QFont>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/LabelStagger.h"
#include "core/Occupancy.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// Where an end that an axis can't place is put, before it is cut off at the plot's edge.
constexpr double kFarPixels = 1e6;
// Labels of lines and spans: the text's padding in its box, the box's distance from the plot's
// edge along the line, and from the line itself.
constexpr double kLabelPaddingX = 4.0;
constexpr double kLabelPaddingY = 1.0;
constexpr double kLabelRadius   = 3.0;
constexpr double kLabelInset    = 6.0;
constexpr double kLabelGap      = 3.0;
// Event flags: padding, the space between flags in a row and between rows, and the share of the
// plot's height the rows may take.
constexpr double kFlagPaddingX = 5.0;
constexpr double kFlagPaddingY = 1.0;
constexpr double kFlagGap      = 4.0;
constexpr double kFlagRowGap   = 2.0;
constexpr double kFlagTop      = 2.0;
constexpr double kFlagRowShare = 0.5;
// Text annotations: padding, and how far outside the plot a point may be for its text to be drawn.
constexpr double kTextPaddingX = 4.0;
constexpr double kTextPaddingY = 2.0;
constexpr double kTextReach    = 4096.0;
// Arrows: the gaps left at the text and at the point, the head, and the shaft's width relative to
// other annotation lines.
constexpr double kArrowBoxGap     = 2.0;
constexpr double kArrowPointGap   = 4.0;
constexpr double kArrowHeadLength = 8.0;
constexpr double kArrowHeadWidth  = 6.0;
constexpr double kArrowWidthScale = 1.25;

core::PixelBox toBox(const QRectF& rect)
{
    return {.left = rect.left(), .top = rect.top(), .right = rect.right(), .bottom = rect.bottom()};
}

// The pixel of @p value, or far beyond the end of the axis it lies past where the axis can't place
// it (infinite, or <= 0 on a log axis). NaN for NaN.
double edgePixel(const core::AxisMapping& mapping, double value)
{
    if (std::isnan(value))
    {
        return value;
    }
    const double pixel = mapping.toPixel(value);
    if (std::isfinite(pixel))
    {
        return pixel;
    }
    const double direction = mapping.pixelEnd() >= mapping.pixelStart() ? 1.0 : -1.0;
    return value == std::numeric_limits<double>::infinity()
               ? mapping.pixelEnd() + (direction * kFarPixels)
               : mapping.pixelStart() - (direction * kFarPixels);
}

// Where a line @p width wide near @p position is sharpest: centered on a device pixel if it covers
// an odd number of them, else between two.
double crispLine(double position, double width, double devicePixelRatio)
{
    const double devicePixels = std::max(1.0, std::round(width * devicePixelRatio));
    return std::fmod(devicePixels, 2.0) == 1.0
               ? crispPixel(position, devicePixelRatio)
               : std::round(position * devicePixelRatio) / devicePixelRatio;
}

// A box of @p size with the part @p alignment names at @p anchor.
QRectF boxAt(QPointF anchor, QSizeF size, Qt::Alignment alignment)
{
    double left = anchor.x() - (size.width() / 2.0);
    if (alignment.testFlag(Qt::AlignLeft))
    {
        left = anchor.x();
    }
    else if (alignment.testFlag(Qt::AlignRight))
    {
        left = anchor.x() - size.width();
    }
    double top = anchor.y() - (size.height() / 2.0);
    if (alignment.testFlag(Qt::AlignTop))
    {
        top = anchor.y();
    }
    else if (alignment.testFlag(Qt::AlignBottom))
    {
        top = anchor.y() - size.height();
    }
    return {QPointF(left, top), size};
}

// A box of @p size inside @p area where @p alignment says (the top left by default).
QRectF boxIn(const QRectF& area, QSizeF size, Qt::Alignment alignment)
{
    double left = area.left();
    if (alignment.testFlag(Qt::AlignHCenter))
    {
        left = area.center().x() - (size.width() / 2.0);
    }
    else if (alignment.testFlag(Qt::AlignRight))
    {
        left = area.right() - size.width();
    }
    double top = area.top();
    if (alignment.testFlag(Qt::AlignVCenter))
    {
        top = area.center().y() - (size.height() / 2.0);
    }
    else if (alignment.testFlag(Qt::AlignBottom))
    {
        top = area.bottom() - size.height();
    }
    return {QPointF(left, top), size};
}

// How bright a color is to the eye (WCAG relative luminance), from 0 to 1.
double luminance(const QColor& color)
{
    const auto linear = [](float channel) {
        const auto value = static_cast<double>(channel);
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return (0.2126 * linear(color.redF())) + (0.7152 * linear(color.greenF())) +
           (0.0722 * linear(color.blueF()));
}

// The WCAG contrast ratio between two colors, from 1 to 21.
double contrast(const QColor& a, const QColor& b)
{
    const double first  = luminance(a);
    const double second = luminance(b);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

// Whichever of the theme's background and text colors is easier to read on @p fill.
QColor textOn(const QColor& fill, const Theme& theme)
{
    return contrast(theme.background, fill) >= contrast(theme.text, fill) ? theme.background
                                                                          : theme.text;
}

// What every drawing function here needs.
struct Context
{
    QPainter*         painter;
    const PlotLayout* layout;
    const Theme*      theme;
    TextPainter*      text;
    core::Occupancy*  occupancy;

    void occupy(const QRectF& rect) const
    {
        if (occupancy != nullptr)
        {
            occupancy->addBox(toBox(rect));
        }
    }

    // Draws @p label in @p color centered in @p box, over a rounded @p fill.
    void drawBoxedText(const QString& label, const QColor& color, const QRectF& box,
                       const QColor& fill) const
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(box, kLabelRadius, kLabelRadius);
        painter->setBrush(Qt::NoBrush);
        text->draw(*painter, label, layout->annotationFont, color, box, Qt::AlignCenter);
    }
};

// Spans
// -----------------------------------------------------------------------------------------------------------

// The part of @p span inside the plot area; empty if there is none.
QRectF spanRect(const ShadedSpan& span, const PlotLayout& layout)
{
    const QRectF&            plot     = layout.plot;
    const bool               vertical = span.orientation() == Qt::Vertical;
    const core::AxisMapping& mapping  = vertical ? layout.x.mapping : layout.yFor(span).mapping;
    const double             a        = edgePixel(mapping, span.min());
    const double             b        = edgePixel(mapping, span.max());
    if (std::isnan(a) || std::isnan(b))
    {
        return {};
    }
    if (vertical)
    {
        return {QPointF(std::clamp(std::min(a, b), plot.left(), plot.right()), plot.top()),
                QPointF(std::clamp(std::max(a, b), plot.left(), plot.right()), plot.bottom())};
    }
    return {QPointF(plot.left(), std::clamp(std::min(a, b), plot.top(), plot.bottom())),
            QPointF(plot.right(), std::clamp(std::max(a, b), plot.top(), plot.bottom()))};
}

void drawSpan(const Context& context, const ShadedSpan& span)
{
    const QRectF rect = spanRect(span, *context.layout);
    if (rect.isEmpty())
    {
        return;
    }
    QColor fill = span.color();
    fill.setAlphaF(static_cast<float>(static_cast<double>(fill.alphaF()) * span.opacity()));
    context.painter->fillRect(rect, fill);
}

void drawSpanLabel(const Context& context, const ShadedSpan& span)
{
    const QRectF rect = spanRect(span, *context.layout);
    if (span.label().isEmpty() || rect.isEmpty())
    {
        return;
    }
    const QSizeF size = context.text->size(span.label(), context.layout->annotationFont);
    const QRectF box  = boxIn(rect.adjusted(kLabelInset, kLabelGap, -kLabelInset, -kLabelGap), size,
                              span.labelAlignment());
    context.text->draw(*context.painter, span.label(), context.layout->annotationFont,
                       context.theme->secondaryText, box, Qt::AlignCenter);
    context.occupy(box);
}

// Lines
// -----------------------------------------------------------------------------------------------------------

// A line across the plot at one pixel of an axis.
struct LinePlace
{
    bool   vertical = false;
    double pixel    = 0.0;
};

// Where a line @p width wide at @p value of the x axis (@p vertical) or of the y axis laid out as
// @p mapping is; nothing if it is out of view.
std::optional<LinePlace> placeLine(const QRectF& plot, const core::AxisMapping& mapping,
                                   bool vertical, double value, double width)
{
    const double pixel = mapping.toPixel(value);
    const double low   = vertical ? plot.left() : plot.top();
    const double high  = vertical ? plot.right() : plot.bottom();
    if (!std::isfinite(pixel) || pixel < low - width || pixel > high + width)
    {
        return std::nullopt;
    }
    return LinePlace{.vertical = vertical, .pixel = pixel};
}

std::optional<LinePlace> placeOf(const ReferenceLine& line, const PlotLayout& layout)
{
    const bool vertical = line.orientation() == Qt::Vertical;
    return placeLine(layout.plot, vertical ? layout.x.mapping : layout.yFor(line).mapping, vertical,
                     line.value(), line.lineWidth());
}

std::optional<LinePlace> placeOf(const EventMarker& event, const PlotLayout& layout,
                                 const Theme& theme)
{
    return placeLine(layout.plot, layout.x.mapping, true, event.x(), theme.annotationLineWidth);
}

void strokeLine(const Context& context, LinePlace place, const QPen& pen)
{
    const QRectF& plot = context.layout->plot;
    const double  at   = crispLine(place.pixel, pen.widthF(), context.layout->devicePixelRatio);
    context.painter->setPen(pen);
    QRectF covered;
    if (place.vertical)
    {
        context.painter->drawLine(QPointF(at, plot.top()), QPointF(at, plot.bottom()));
        covered = QRectF(at - (pen.widthF() / 2.0), plot.top(), pen.widthF(), plot.height());
    }
    else
    {
        context.painter->drawLine(QPointF(plot.left(), at), QPointF(plot.right(), at));
        covered = QRectF(plot.left(), at - (pen.widthF() / 2.0), plot.width(), pen.widthF());
    }
    context.occupy(covered);
}

void drawLine(const Context& context, const ReferenceLine& line)
{
    if (const std::optional<LinePlace> place = placeOf(line, *context.layout))
    {
        strokeLine(context, *place,
                   QPen(line.color(), line.lineWidth(), line.lineStyle(), Qt::FlatCap));
    }
}

void drawEventLine(const Context& context, const EventMarker& event)
{
    if (const std::optional<LinePlace> place = placeOf(event, *context.layout, *context.theme))
    {
        strokeLine(
            context, *place,
            QPen(event.color(), context.theme->annotationLineWidth, Qt::SolidLine, Qt::FlatCap));
    }
}

// The box of a label of @p size beside a line at @p place: at the end of the line and on the side
// of it that @p alignment says, or on the other side where that one has no room.
QRectF lineLabelBox(const QRectF& plot, LinePlace place, QSizeF size, Qt::Alignment alignment)
{
    if (place.vertical)
    {
        const QRectF along     = boxIn(plot.adjusted(0.0, kLabelInset, 0.0, -kLabelInset), size,
                                       alignment & Qt::AlignVertical_Mask);
        const double right     = place.pixel + kLabelGap;
        const double left      = place.pixel - kLabelGap - size.width();
        const bool   fitsRight = right + size.width() <= plot.right();
        const bool   fitsLeft  = left >= plot.left();
        const bool   onLeft =
            alignment.testFlag(Qt::AlignLeft) ? fitsLeft || !fitsRight : !fitsRight && fitsLeft;
        return {QPointF(onLeft ? left : right, along.top()), size};
    }
    const QRectF along     = boxIn(plot.adjusted(kLabelInset, 0.0, -kLabelInset, 0.0), size,
                                   alignment & Qt::AlignHorizontal_Mask);
    const double above     = place.pixel - kLabelGap - size.height();
    const double below     = place.pixel + kLabelGap;
    const bool   fitsAbove = above >= plot.top();
    const bool   fitsBelow = below + size.height() <= plot.bottom();
    const bool   onBottom =
        alignment.testFlag(Qt::AlignBottom) ? fitsBelow || !fitsAbove : !fitsAbove && fitsBelow;
    return {QPointF(along.left(), onBottom ? below : above), size};
}

void drawLineLabel(const Context& context, const ReferenceLine& line)
{
    const std::optional<LinePlace> place = placeOf(line, *context.layout);
    if (line.label().isEmpty() || !place)
    {
        return;
    }
    const QSizeF size = context.text->size(line.label(), context.layout->annotationFont) +
                        QSizeF(2.0 * kLabelPaddingX, 2.0 * kLabelPaddingY);
    const QRectF box  = lineLabelBox(context.layout->plot, *place, size, line.labelAlignment());
    // The text in a text color, like every label: the line beside it carries the color.
    context.drawBoxedText(line.label(), context.theme->secondaryText, box,
                          context.theme->legendBackground);
    context.occupy(box);
}

// Event flags
// ------------------------------------------------------------------------------------------------------

// The flags of the plot's events, each in the row that keeps it clear of its neighbors. Events out
// of view count too, so that panning doesn't move flags between rows.
void drawEventFlags(const Context& context, const QList<Annotation*>& annotations)
{
    const QRectF&                   plot = context.layout->plot;
    const QFont&                    font = context.layout->annotationFont;
    std::vector<const EventMarker*> events;
    std::vector<core::LabelSpan>    spans;
    double                          height = 0.0;
    for (const Annotation* annotation : annotations)
    {
        const auto* event = qobject_cast<const EventMarker*>(annotation);
        if (event == nullptr || !event->isVisible() || event->label().isEmpty())
        {
            continue;
        }
        const double pixel = context.layout->x.mapping.toPixel(event->x());
        if (!std::isfinite(pixel))
        {
            continue;
        }
        const QSizeF size  = context.text->size(event->label(), font);
        const double width = size.width() + (2.0 * kFlagPaddingX);
        // A flag flies to the right of its line, or to the left where it would leave the plot,
        // flush with the pixel column the line is drawn in.
        const double dpr    = context.layout->devicePixelRatio;
        const double column = std::floor(pixel * dpr) / dpr;
        const bool   flipped =
            pixel <= plot.right() && column + width > plot.right() && column - width >= plot.left();
        const double left = flipped ? column + (1.0 / dpr) - width : column;
        events.push_back(event);
        spans.push_back({.left = left, .right = left + width});
        height = std::max(height, size.height() + (2.0 * kFlagPaddingY));
    }
    if (events.empty())
    {
        return;
    }
    const double pitch   = height + kFlagRowGap;
    const int    maxRows = std::max(1, static_cast<int>(plot.height() * kFlagRowShare / pitch));
    const std::vector<int> rows = core::staggerLabels(spans, kFlagGap, maxRows);
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const QRectF box(spans[i].left, plot.top() + kFlagTop + (rows[i] * pitch),
                         spans[i].right - spans[i].left, height);
        if (rows[i] == core::kNoRow || !box.intersects(plot))
        {
            continue;
        }
        const QColor fill = events[i]->color();
        context.drawBoxedText(events[i]->label(), textOn(fill, *context.theme), box, fill);
        context.occupy(box);
    }
}

// Text
// ------------------------------------------------------------------------------------------------------------

// An arrow from the edge of @p box toward @p target, if there is room for one between them.
void drawArrow(const Context& context, const QRectF& box, QPointF target, const QColor& color)
{
    const QPointF center = box.center();
    const QPointF delta  = target - center;
    const double  length = std::hypot(delta.x(), delta.y());
    // How far along the way to the target the box (and a gap around it) ends.
    const double halfWidth  = (box.width() / 2.0) + kArrowBoxGap;
    const double halfHeight = (box.height() / 2.0) + kArrowBoxGap;
    const double leaves     = std::min(
        delta.x() != 0.0 ? halfWidth / std::abs(delta.x()) : std::numeric_limits<double>::max(),
        delta.y() != 0.0 ? halfHeight / std::abs(delta.y()) : std::numeric_limits<double>::max());
    if (!(length > 0.0) || leaves >= 1.0)
    {
        return;  // the point is under the text
    }
    const QPointF unit  = delta / length;
    const QPointF start = center + (delta * leaves);
    const QPointF tip   = target - (unit * kArrowPointGap);
    if (QPointF::dotProduct(tip - start, unit) < kArrowHeadLength)
    {
        return;
    }
    const QPointF base    = tip - (unit * kArrowHeadLength);
    const QPointF side    = QPointF(-unit.y(), unit.x()) * (kArrowHeadWidth / 2.0);
    QPainter&     painter = *context.painter;
    painter.setPen(QPen(color, context.theme->annotationLineWidth * kArrowWidthScale, Qt::SolidLine,
                        Qt::FlatCap));
    painter.drawLine(start, base);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const std::array<QPointF, 3> head{tip, base + side, base - side};
    painter.drawPolygon(head.data(), static_cast<int>(head.size()));
    painter.setBrush(Qt::NoBrush);
}

void drawText(const Context& context, const TextAnnotation& annotation)
{
    const PlotLayout& layout = *context.layout;
    const QPointF     target(layout.x.mapping.toPixel(annotation.position().x()),
                             layout.yFor(annotation).mapping.toPixel(annotation.position().y()));
    const QRectF reach = layout.plot.adjusted(-kTextReach, -kTextReach, kTextReach, kTextReach);
    if (annotation.text().isEmpty() || !std::isfinite(target.x()) || !std::isfinite(target.y()) ||
        !reach.contains(target))
    {
        return;
    }
    const QSizeF size = context.text->size(annotation.text(), layout.annotationFont) +
                        QSizeF(2.0 * kTextPaddingX, 2.0 * kTextPaddingY);
    const QRectF box  = boxAt(target + annotation.offset(), size, annotation.alignment());
    if (!reach.contains(box.center()))
    {
        return;
    }
    const QColor color = annotation.color();
    if (annotation.isArrowVisible())
    {
        drawArrow(context, box, target, color);
    }
    if (annotation.isBackgroundVisible())
    {
        context.drawBoxedText(annotation.text(), color, box, context.theme->legendBackground);
    }
    else
    {
        context.text->draw(*context.painter, annotation.text(), layout.annotationFont, color, box,
                           Qt::AlignCenter);
    }
    context.occupy(box);
}

}  // namespace

void drawAnnotations(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                     TextPainter& text, AnnotationLayer layer, core::Occupancy* occupancy)
{
    const QList<Annotation*> annotations = plot.annotations();
    if (annotations.isEmpty())
    {
        return;
    }
    const Context context{
        .painter   = &painter,
        .layout    = &layout,
        .theme     = &plot.theme(),
        .text      = &text,
        .occupancy = occupancy,
    };
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const Annotation* annotation : annotations)
    {
        if (!annotation->isVisible() || annotation->layer() != layer)
        {
            continue;
        }
        if (const auto* span = qobject_cast<const ShadedSpan*>(annotation))
        {
            drawSpan(context, *span);
        }
        else if (const auto* line = qobject_cast<const ReferenceLine*>(annotation))
        {
            drawLine(context, *line);
        }
        else if (const auto* event = qobject_cast<const EventMarker*>(annotation))
        {
            drawEventLine(context, *event);
        }
        else if (const auto* note = qobject_cast<const TextAnnotation*>(annotation))
        {
            drawText(context, *note);
        }
    }
    painter.restore();
}

void drawAnnotationLabels(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                          TextPainter& text, core::Occupancy* occupancy)
{
    const QList<Annotation*> annotations = plot.annotations();
    if (annotations.isEmpty())
    {
        return;
    }
    const Context context{
        .painter   = &painter,
        .layout    = &layout,
        .theme     = &plot.theme(),
        .text      = &text,
        .occupancy = occupancy,
    };
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const Annotation* annotation : annotations)
    {
        if (!annotation->isVisible())
        {
            continue;
        }
        if (const auto* span = qobject_cast<const ShadedSpan*>(annotation))
        {
            drawSpanLabel(context, *span);
        }
        else if (const auto* line = qobject_cast<const ReferenceLine*>(annotation))
        {
            drawLineLabel(context, *line);
        }
    }
    drawEventFlags(context, annotations);  // on top: they are the most crowded
    painter.restore();
}

}  // namespace rocketplot
