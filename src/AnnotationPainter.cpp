#include "AnnotationPainter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QIcon>
#include <QList>
#include <QPaintEngine>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "FlagLayout.h"
#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "TextPainter.h"
#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/Occupancy.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/RectAnnotation.h"
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
// How far from a whole number of device pixels a line width may be and count as one.
constexpr double kWholePixelTolerance = 0.01;
// Labels of lines and spans: the text's padding in its box, the box's distance from the plot's
// edge along the line, and from the line itself.
constexpr double kLabelPaddingX = 4.0;
constexpr double kLabelPaddingY = 1.0;
constexpr double kLabelRadius   = 3.0;
constexpr double kLabelInset    = 6.0;
constexpr double kLabelGap      = 3.0;
// Icons in drawings (SVG, PDF, print) are images of at least this many pixels per unit.
constexpr double kDrawingIconRatio = 4.0;
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
    QPainter*                   painter   = nullptr;
    const PlotLayout*           layout    = nullptr;
    const Theme*                theme     = nullptr;
    TextPainter*                text      = nullptr;
    core::Occupancy*            occupancy = nullptr;
    std::vector<AnnotationHit>* hits      = nullptr;
    QRectF                      clip;  // where what is drawn shows

    void occupy(const QRectF& rect) const
    {
        if (occupancy != nullptr)
        {
            occupancy->addBox(toBox(rect));
        }
    }

    // Records that @p annotation drew @p box, as far as it shows.
    void hit(const Annotation& annotation, const QRectF& box) const
    {
        const QRectF shown = box.intersected(clip);
        if (hits != nullptr && !shown.isEmpty())
        {
            hits->push_back({.annotation = &annotation, .box = shown});
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

// The region between the values @p x and @p y (either may run to infinity) in pixels, cut off at
// the edges of @p bounds; empty if it is outside them, null if it has a NaN end.
QRectF pixelRegion(Range x, Range y, const AnnotationMappings& mappings, const QRectF& bounds)
{
    const double left   = edgePixel(mappings.x, x.min);
    const double right  = edgePixel(mappings.x, x.max);
    const double top    = edgePixel(mappings.y, y.min);
    const double bottom = edgePixel(mappings.y, y.max);
    if (std::isnan(left) || std::isnan(right) || std::isnan(top) || std::isnan(bottom))
    {
        return {};
    }
    return {QPointF(std::clamp(std::min(left, right), bounds.left(), bounds.right()),
                    std::clamp(std::min(top, bottom), bounds.top(), bounds.bottom())),
            QPointF(std::clamp(std::max(left, right), bounds.left(), bounds.right()),
                    std::clamp(std::max(top, bottom), bounds.top(), bounds.bottom()))};
}

// The part of @p span inside the plot area; empty if there is none.
QRectF spanRect(const ShadedSpan& span, const PlotLayout& layout)
{
    constexpr Range kAcross{.min = -std::numeric_limits<double>::infinity(),
                            .max = std::numeric_limits<double>::infinity()};
    const bool      vertical = span.orientation() == Qt::Vertical;
    return pixelRegion(vertical ? span.range() : kAcross, vertical ? kAcross : span.range(),
                       layout.mappingsFor(span), layout.plot);
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

// @p label inside @p region (the part of a span or rectangle in view) where @p alignment says.
void drawRegionLabel(const Context& context, const Annotation& annotation, const QRectF& region,
                     const QString& label, Qt::Alignment alignment)
{
    if (label.isEmpty() || region.isEmpty())
    {
        return;
    }
    const QSizeF size = context.text->size(label, context.layout->annotationFont);
    const QRectF box =
        boxIn(region.adjusted(kLabelInset, kLabelGap, -kLabelInset, -kLabelGap), size, alignment);
    context.text->draw(*context.painter, label, context.layout->annotationFont,
                       context.theme->secondaryText, box, Qt::AlignCenter);
    context.occupy(box);
    context.hit(annotation, box);
}

void drawSpanLabel(const Context& context, const ShadedSpan& span)
{
    drawRegionLabel(context, span, spanRect(span, *context.layout), span.label(),
                    span.labelAlignment());
}

// Rectangles
// ------------------------------------------------------------------------------------------------------

// The part of @p rect inside the plot area; empty if there is none.
QRectF visibleRect(const RectAnnotation& rect, const PlotLayout& layout)
{
    return pixelRegion(rect.xRange(), rect.yRange(), layout.mappingsFor(rect), layout.plot);
}

void drawRect(const Context& context, const RectAnnotation& rect)
{
    const PlotLayout& layout = *context.layout;
    const double      width  = rect.lineWidth();
    const double      dpr    = layout.devicePixelRatio;
    // Where the outline goes, an edge out of view just past the plot's edge (and so clipped away).
    const QRectF reach = layout.plot.adjusted(-2.0 * width, -2.0 * width, 2.0 * width, 2.0 * width);
    const QRectF region =
        pixelRegion(rect.xRange(), rect.yRange(), layout.mappingsFor(rect), reach);
    if (region.isEmpty())
    {
        return;  // out of view, or with a NaN edge
    }
    if (const QRectF visible = region.intersected(layout.plot);
        rect.fillOpacity() > 0.0 && !visible.isEmpty())
    {
        QColor fill = rect.color();
        fill.setAlphaF(static_cast<float>(static_cast<double>(fill.alphaF()) * rect.fillOpacity()));
        context.painter->fillRect(snappedToDevice(visible, dpr), fill);
    }
    if (rect.lineStyle() == Qt::NoPen)
    {
        return;
    }
    const QRectF outline(
        QPointF(crispLine(region.left(), width, dpr), crispLine(region.top(), width, dpr)),
        QPointF(crispLine(region.right(), width, dpr), crispLine(region.bottom(), width, dpr)));
    // As for lines: a width of whole device pixels covers whole pixels, without antialiasing.
    const double deviceWidth = width * dpr;
    const bool wholePixels = std::abs(deviceWidth - std::round(deviceWidth)) < kWholePixelTolerance;
    context.painter->setRenderHint(QPainter::Antialiasing, !wholePixels);
    context.painter->setPen(
        QPen(rect.color(), width, rect.lineStyle(), Qt::FlatCap, Qt::MiterJoin));
    context.painter->setBrush(Qt::NoBrush);
    context.painter->drawRect(outline);
    context.painter->setRenderHint(QPainter::Antialiasing, true);
    // What a legend should keep clear of: the four edges.
    const double half  = width / 2.0;
    const QRectF outer = outline.adjusted(-half, -half, half, half);
    for (const QRectF& edge : {QRectF(outer.left(), outer.top(), outer.width(), width),
                               QRectF(outer.left(), outer.bottom() - width, outer.width(), width),
                               QRectF(outer.left(), outer.top(), width, outer.height()),
                               QRectF(outer.right() - width, outer.top(), width, outer.height())})
    {
        if (const QRectF shown = edge.intersected(layout.plot); !shown.isEmpty())
        {
            context.occupy(shown);
        }
    }
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
    const bool               vertical = line.orientation() == Qt::Vertical;
    const AnnotationMappings mappings = layout.mappingsFor(line);
    return placeLine(layout.plot, vertical ? mappings.x : mappings.y, vertical, line.value(),
                     line.lineWidth());
}

std::optional<LinePlace> placeOf(const EventMarker& event, const PlotLayout& layout,
                                 const Theme& theme)
{
    return placeLine(layout.plot, layout.mappingsFor(event).x, true, event.x(),
                     theme.annotationLineWidth);
}

void strokeLine(const Context& context, LinePlace place, const QPen& pen)
{
    const QRectF& plot = context.layout->plot;
    const double  dpr  = context.layout->devicePixelRatio;
    const double  at   = crispLine(place.pixel, pen.widthF(), dpr);
    // A line along an axis that is a whole number of device pixels wide covers whole pixels:
    // antialiasing wouldn't change it, and takes ten times as long.
    const double deviceWidth = pen.widthF() * dpr;
    const bool wholePixels = std::abs(deviceWidth - std::round(deviceWidth)) < kWholePixelTolerance;
    context.painter->setRenderHint(QPainter::Antialiasing, !wholePixels);
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
    context.painter->setRenderHint(QPainter::Antialiasing, true);
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
    context.hit(line, box);
}

// Event flags
// ------------------------------------------------------------------------------------------------------

// The flags of the plot's events, as laid out, where they show in @p area (the plot area, or the
// band above it).
void drawEventFlags(const Context& context, const FlagLayout& flags, const QRectF& area)
{
    QPainter&    painter = *context.painter;
    const QFont& font    = context.layout->annotationFont;
    const double dpr     = context.layout->devicePixelRatio;
    for (const EventFlag& flag : flags.flags)
    {
        if (!flag.box.intersects(area))
        {
            continue;
        }
        const EventMarker& event = *flag.event;
        const QColor       fill  = event.color();
        // Drawn as a horizontal flag, turned to read upward for a vertical one.
        painter.save();
        if (flags.vertical)
        {
            painter.translate(flag.box.bottomLeft());
            painter.rotate(-90.0);
        }
        else
        {
            painter.translate(flag.box.topLeft());
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(QPointF(), flag.size), kLabelRadius, kLabelRadius);
        painter.setBrush(Qt::NoBrush);
        if (!flag.icon.isNull())
        {
            drawIcon(painter, event.icon(), flag.icon, dpr);
        }
        if (!flag.text.isNull())
        {
            context.text->draw(painter, event.label(), font, textOn(fill, *context.theme),
                               flag.text, Qt::AlignLeft | Qt::AlignVCenter, flags.vertical);
        }
        painter.restore();
        context.occupy(flag.box);
        context.hit(event, flag.box);
    }
}

// Icons
// -----------------------------------------------------------------------------------------------------------

void drawIconAnnotation(const Context& context, const IconAnnotation& annotation)
{
    const PlotLayout&        layout   = *context.layout;
    const AnnotationMappings mappings = layout.mappingsFor(annotation);
    const QPointF            target(mappings.x.toPixel(annotation.position().x()),
                                    mappings.y.toPixel(annotation.position().y()));
    const QRectF reach = layout.plot.adjusted(-kTextReach, -kTextReach, kTextReach, kTextReach);
    if (annotation.icon().isNull() || !std::isfinite(target.x()) || !std::isfinite(target.y()) ||
        !reach.contains(target))
    {
        return;
    }
    const QRectF box = snappedToDevice(
        boxAt(target + annotation.offset(), annotation.size(), annotation.alignment()),
        layout.devicePixelRatio);
    drawIcon(*context.painter, annotation.icon(), box, layout.devicePixelRatio);
    context.occupy(box.intersected(layout.plot));
    context.hit(annotation, box);
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
    const PlotLayout&        layout   = *context.layout;
    const AnnotationMappings mappings = layout.mappingsFor(annotation);
    const QPointF            target(mappings.x.toPixel(annotation.position().x()),
                                    mappings.y.toPixel(annotation.position().y()));
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
        context.drawBoxedText(annotation.text(), color, box, annotation.backgroundColor());
    }
    else
    {
        context.text->draw(*context.painter, annotation.text(), layout.annotationFont, color, box,
                           Qt::AlignCenter);
    }
    if (annotation.isFrameVisible())
    {
        // A hairline on whole device pixels, as the legend's.
        const double pixel  = 1.0 / layout.devicePixelRatio;
        const QRectF framed = snappedToDevice(box, layout.devicePixelRatio);
        context.painter->setPen(hairlinePen(context.theme->legendBorder));
        context.painter->setBrush(Qt::NoBrush);
        context.painter->drawRoundedRect(
            framed.adjusted(pixel / 2.0, pixel / 2.0, -pixel / 2.0, -pixel / 2.0), kLabelRadius,
            kLabelRadius);
    }
    context.occupy(box);
    context.hit(annotation, box);
}

}  // namespace

void drawAnnotations(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                     TextPainter& text, AnnotationLayer layer, const AnnotationRecord& record)
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
        .occupancy = record.occupancy,
        .hits      = record.hits,
        .clip      = layout.plot,
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
        else if (const auto* rect = qobject_cast<const RectAnnotation*>(annotation))
        {
            drawRect(context, *rect);
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
        else if (const auto* icon = qobject_cast<const IconAnnotation*>(annotation))
        {
            drawIconAnnotation(context, *icon);
        }
    }
    painter.restore();
}

void drawAnnotationLabels(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                          TextPainter& text, const AnnotationRecord& record)
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
        .occupancy = record.occupancy,
        .hits      = record.hits,
        .clip      = layout.plot,
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
        else if (const auto* rect = qobject_cast<const RectAnnotation*>(annotation))
        {
            drawRegionLabel(context, *rect, visibleRect(*rect, *context.layout), rect->label(),
                            rect->labelAlignment());
        }
        else if (const auto* line = qobject_cast<const ReferenceLine*>(annotation))
        {
            drawLineLabel(context, *line);
        }
    }
    if (layout.flagBand.isNull())
    {
        drawEventFlags(context, layout.flags, layout.plot);  // on top: they are the most crowded
    }
    painter.restore();
}

void drawIcon(QPainter& painter, const QIcon& icon, const QRectF& box, double devicePixelRatio)
{
    const QPaintEngine* engine = painter.paintEngine();
    const bool          raster = engine == nullptr || engine->type() == QPaintEngine::Raster;
    const double  ratio = raster ? devicePixelRatio : std::max(devicePixelRatio, kDrawingIconRatio);
    const QSize   size(std::max(1, static_cast<int>(std::lround(box.width()))),
                       std::max(1, static_cast<int>(std::lround(box.height()))));
    const QPixmap pixmap = icon.pixmap(size, ratio);
    if (!pixmap.isNull())
    {
        painter.drawPixmap(box, pixmap, QRectF(QPointF(), QSizeF(pixmap.size())));
    }
}

void drawFlagBand(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                  TextPainter& text, const AnnotationRecord& record)
{
    if (layout.flagBand.isNull() || layout.flags.flags.empty())
    {
        return;
    }
    // From the top of the band down to the plot area, as wide as the plot area.
    const QRectF  area(layout.flagBand.topLeft(), QPointF(layout.plot.right(), layout.plot.top()));
    const Context context{
        .painter   = &painter,
        .layout    = &layout,
        .theme     = &plot.theme(),
        .text      = &text,
        .occupancy = nullptr,  // the legend is inside the plot area
        .hits      = record.hits,
        .clip      = area,
    };
    painter.save();
    painter.setClipRect(area, Qt::IntersectClip);
    // Each flag's line, from the flag down to the plot area, where the event's line goes on.
    const double width       = plot.theme().annotationLineWidth;
    const double dpr         = layout.devicePixelRatio;
    const double deviceWidth = width * dpr;
    painter.setRenderHint(QPainter::Antialiasing,
                          std::abs(deviceWidth - std::round(deviceWidth)) >= kWholePixelTolerance);
    for (const EventFlag& flag : layout.flags.flags)
    {
        const double x = crispLine(flag.line, width, dpr);
        if (x < layout.plot.left() || x > layout.plot.right())
        {
            continue;
        }
        painter.setPen(QPen(flag.event->color(), width, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(x, flag.box.bottom()), QPointF(x, layout.plot.top()));
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawEventFlags(context, layout.flags, area);
    painter.restore();
}

}  // namespace rocketplot
