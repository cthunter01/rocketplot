#include "MarkerPainter.h"

#include <QBrush>
#include <QColor>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "core/Decimator.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// Sprites are cleared when the cache grows past this many styles (they are cheap to make again).
constexpr qsizetype kMaxSprites = 256;
// A square looks bigger than a circle of the same diameter; shrink it to match visually.
constexpr double kSquareScale = 0.85;
// Stroke width of CROSS and PLUS, relative to their size.
constexpr double kStrokeFraction = 0.22;

bool isStroked(Marker marker)
{
    return marker == Marker::CROSS || marker == Marker::PLUS;
}

double strokeWidth(const MarkerStyle& style)
{
    return std::max(1.5, style.size * kStrokeFraction);
}

// Draws one marker centered on (0, 0) of the painter's current coordinates.
void paintMarker(QPainter& painter, const MarkerStyle& style)
{
    const QPainterPath path = markerPath(style.shape, style.size);
    if (path.isEmpty())
    {
        return;
    }
    if (isStroked(style.shape))
    {
        const double width = strokeWidth(style);
        if (style.ringWidth > 0.0)
        {
            painter.strokePath(path, QPen(style.ring, width + (2.0 * style.ringWidth),
                                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
        painter.strokePath(path,
                           QPen(style.color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        return;
    }
    if (style.ringWidth > 0.0)
    {
        // A stroke twice the ring width, centered on the outline: its outer half is the ring.
        painter.strokePath(path, QPen(style.ring, 2.0 * style.ringWidth, Qt::SolidLine,
                                      Qt::RoundCap, Qt::RoundJoin));
    }
    painter.fillPath(path, style.color);
}

}  // namespace

QPainterPath markerPath(Marker marker, double size)
{
    const double r = size / 2.0;
    QPainterPath path;
    switch (marker)
    {
        case Marker::NONE:
            break;
        case Marker::CIRCLE:
            path.addEllipse(QPointF(0.0, 0.0), r, r);
            break;
        case Marker::SQUARE:
        {
            const double h = r * kSquareScale;
            path.addRect(QRectF(-h, -h, 2.0 * h, 2.0 * h));
            break;
        }
        case Marker::DIAMOND:
            path.moveTo(0.0, -r);
            path.lineTo(r, 0.0);
            path.lineTo(0.0, r);
            path.lineTo(-r, 0.0);
            path.closeSubpath();
            break;
        case Marker::TRIANGLE:
        {
            // Equilateral, centered on its centroid, pointing up.
            const double h = r * 1.5;
            const double w = h / std::numbers::sqrt3;
            path.moveTo(0.0, -r);
            path.lineTo(w, h - r);
            path.lineTo(-w, h - r);
            path.closeSubpath();
            break;
        }
        case Marker::CROSS:
        {
            const double d = r * std::sqrt(0.5);
            path.moveTo(-d, -d);
            path.lineTo(d, d);
            path.moveTo(-d, d);
            path.lineTo(d, -d);
            break;
        }
        case Marker::PLUS:
            path.moveTo(-r, 0.0);
            path.lineTo(r, 0.0);
            path.moveTo(0.0, -r);
            path.lineTo(0.0, r);
            break;
    }
    return path;
}

void MarkerPainter::draw(QPainter& painter, std::span<const core::PixelPoint> centers,
                         const MarkerStyle& style)
{
    if (style.shape == Marker::NONE || centers.empty())
    {
        return;
    }
    const QPaintEngine* engine = painter.paintEngine();
    if (engine != nullptr && engine->type() == QPaintEngine::Raster)
    {
        const double   dpr    = painter.device()->devicePixelRatioF();
        const QPixmap& pixmap = sprite(style, dpr);
        const double   half   = static_cast<double>(pixmap.width()) / dpr / 2.0;
        for (const core::PixelPoint& center : centers)
        {
            // Whole device pixels keep the copy a plain blit, and the sprite sharp.
            const double left = std::round((center.x - half) * dpr) / dpr;
            const double top  = std::round((center.y - half) * dpr) / dpr;
            painter.drawPixmap(QPointF(left, top), pixmap);
        }
        return;
    }
    for (const core::PixelPoint& center : centers)
    {
        draw(painter, QPointF(center.x, center.y), style);
    }
}

void MarkerPainter::draw(QPainter& painter, QPointF center, const MarkerStyle& style)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(center);
    paintMarker(painter, style);
    painter.restore();
}

const QPixmap& MarkerPainter::sprite(const MarkerStyle& style, double devicePixelRatio)
{
    const QString key = QStringLiteral("%1/%2/%3/%4/%5/%6")
                            .arg(static_cast<int>(style.shape))
                            .arg(style.size)
                            .arg(style.color.rgba())
                            .arg(style.ring.rgba())
                            .arg(style.ringWidth)
                            .arg(devicePixelRatio);
    if (const auto found = m_sprites.constFind(key); found != m_sprites.constEnd())
    {
        return *found;
    }
    if (m_sprites.size() >= kMaxSprites)
    {
        m_sprites.clear();
    }
    // Room for the shape, its ring and antialiasing, in whole device pixels, odd so the center is a
    // pixel center.
    const double extent     = style.size + (2.0 * style.ringWidth) +
                              (isStroked(style.shape) ? strokeWidth(style) : 0.0) + 2.0;
    int          deviceSize = static_cast<int>(std::ceil(extent * devicePixelRatio));
    deviceSize += deviceSize % 2 == 0 ? 1 : 0;

    QPixmap pixmap(deviceSize, deviceSize);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const double center = static_cast<double>(deviceSize) / devicePixelRatio / 2.0;
        painter.translate(center, center);
        paintMarker(painter, style);
    }
    return *m_sprites.insert(key, pixmap);
}

}  // namespace rocketplot
