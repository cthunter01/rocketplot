#include "Overlays.h"

#include <QColor>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <optional>

#include "PlotLayout.h"
#include "PlotRenderer.h"
#include "core/AxisMapping.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"

namespace rocketplot
{

namespace
{

constexpr double kTagPaddingX = 4.0;
constexpr double kTagPaddingY = 1.0;
constexpr double kTagRadius   = 3.0;

// A value tag: @p text on a rounded box of the theme's tag colors.
void drawTag(QPainter& painter, const PlotLayout& layout, const Theme& theme, const QRectF& box,
             const QString& text)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.tagBackground);
    painter.drawRoundedRect(box, kTagRadius, kTagRadius);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(theme.tagText);
    painter.setFont(layout.tickFont);
    painter.drawText(box, Qt::AlignCenter, text);
}

// The size of a tag around @p text.
QRectF tagBox(const QFontMetricsF& metrics, const QString& text)
{
    return {0.0, 0.0, metrics.horizontalAdvance(text) + (2.0 * kTagPaddingX),
            metrics.height() + (2.0 * kTagPaddingY)};
}

}  // namespace

void drawCrosshair(QPainter& painter, const PlotWidget& plot, const PlotLayout& layout,
                   std::optional<QPointF> pointer, std::optional<double> linkedX)
{
    const QRectF& area = layout.plot;
    const double  px   = pointer ? pointer->x() : layout.x.mapping.toPixel(linkedX.value_or(0.0));
    if (!std::isfinite(px) || px < area.left() || px > area.right())
    {
        return;
    }
    const Theme& theme = plot.theme();
    const double dpr   = layout.devicePixelRatio;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(hairlinePen(theme.crosshair));
    const double lineX = crispPixel(px, dpr);
    painter.drawLine(QPointF(lineX, area.top()), QPointF(lineX, area.bottom()));
    if (pointer)
    {
        const double lineY = crispPixel(pointer->y(), dpr);
        painter.drawLine(QPointF(area.left(), lineY), QPointF(area.right(), lineY));
    }

    // Tags over the tick labels, with the coordinates written out in full.
    const QFontMetricsF metrics(layout.tickFont);
    const double        labelGap = theme.tickLength + kTickLabelGap - kTagPaddingY;
    if (layout.x.shown)
    {
        const QString text = readoutLabel(*plot.xAxis(), layout.x.mapping, px);
        QRectF        box  = tagBox(metrics, text);
        const double  left = std::clamp(px - (box.width() / 2.0), layout.bounds.left(),
                                        layout.bounds.right() - box.width());
        box.moveTopLeft(QPointF(left, area.bottom() + labelGap));
        drawTag(painter, layout, theme, box, text);
    }
    const auto yTag = [&](const Axis& axis, const AxisLayout& axisLayout, bool left) {
        if (!pointer || !axisLayout.shown)
        {
            return;
        }
        const QString text = readoutLabel(axis, axisLayout.mapping, pointer->y());
        QRectF        box  = tagBox(metrics, text);
        const double  top =
            std::clamp(pointer->y() - (box.height() / 2.0), area.top() - (box.height() / 2.0),
                       area.bottom() - (box.height() / 2.0));
        const double edge = left ? area.left() - labelGap - box.width() : area.right() + labelGap;
        box.moveTopLeft(QPointF(std::max(edge, layout.bounds.left()), top));
        drawTag(painter, layout, theme, box, text);
    };
    yTag(*plot.yAxis(), layout.y, true);
    yTag(*plot.yAxis2(), layout.y2, false);
    painter.restore();
}

void drawZoomBox(QPainter& painter, const PlotLayout& layout, const Theme& theme, const QRectF& box)
{
    const double dpr   = layout.devicePixelRatio;
    const double pixel = 1.0 / dpr;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(box, theme.zoomBoxFill);
    painter.setPen(hairlinePen(theme.zoomBoxBorder));
    painter.setBrush(Qt::NoBrush);
    // The outline along the pixel rows and columns just inside the box.
    const QRectF outline(
        QPointF(crispPixel(box.left(), dpr), crispPixel(box.top(), dpr)),
        QPointF(crispPixel(box.right() - pixel, dpr), crispPixel(box.bottom() - pixel, dpr)));
    painter.drawRect(outline);
    painter.restore();
}

}  // namespace rocketplot
