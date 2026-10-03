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
#include <QRgb>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>

#include "core/Decimator.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// Sprites are cleared when the cache holds this many, or this many bytes of pixels (they are cheap
// to make again). Markers with their own sizes and colors need one for each combination in view.
constexpr qsizetype kMaxSprites     = 8192;
constexpr qsizetype kMaxSpriteBytes = qsizetype{32} * 1024 * 1024;
// Markers larger than this are drawn as paths: there are few of them in a plot, and a sprite each
// would take too much memory.
constexpr double kMaxSpriteSize = 32.0;
// Per-point sizes are rounded to steps too small to see (finer for small markers) and colors to
// 32 levels per channel, so that markers share sprites: a color scale times a range of sizes then
// needs a few thousand of them, not one per point.
constexpr double kSmallMarker   = 8.0;
constexpr double kSmallSizeStep = 0.5;
constexpr double kSizeStep      = 1.0;
constexpr double kMinimumSize   = 1.0;
constexpr int    kColorLevels   = 31;
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

double coarseSize(double size)
{
    const double step = size < kSmallMarker ? kSmallSizeStep : kSizeStep;
    return std::max(kMinimumSize, std::round(size / step) * step);
}

QRgb coarseColor(QRgb color)
{
    const auto coarse = [](int channel) {
        const int level = ((channel * kColorLevels) + 127) / 255;
        return (level * 255) / kColorLevels;
    };
    return qRgba(coarse(qRed(color)), coarse(qGreen(color)), coarse(qBlue(color)),
                 coarse(qAlpha(color)));
}

}  // namespace

MarkerStyle markerStyle(const Series& series, const Theme& theme)
{
    return {
        .shape     = series.marker(),
        .size      = series.markerSize(),
        .color     = series.color(),
        .ring      = theme.background,
        .ringWidth = theme.markerRingWidth,
    };
}

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

void MarkerPainter::draw(QPainter& painter, std::span<const core::PixelPoint> centers,
                         std::span<const double> sizes, std::span<const QRgb> colors,
                         const MarkerStyle& style)
{
    if (style.shape == Marker::NONE || centers.empty())
    {
        return;
    }
    const QPaintEngine* engine = painter.paintEngine();
    const bool          raster = engine != nullptr && engine->type() == QPaintEngine::Raster;
    const double        dpr    = painter.device()->devicePixelRatioF();
    MarkerStyle         own    = style;
    // The sprite of the marker before, which is often the next one's too.
    QPixmap pixmap;
    double  spriteSize  = -1.0;
    QRgb    spriteColor = 0;
    for (std::size_t i = 0; i < centers.size(); ++i)
    {
        const double size = sizes.empty() ? style.size : sizes[i];
        if (!(size > 0.0))
        {
            continue;
        }
        const QRgb    color = colors.empty() ? style.color.rgba() : colors[i];
        const QPointF center(centers[i].x, centers[i].y);
        if (!raster || size > kMaxSpriteSize)
        {
            own.size  = size;
            own.color = QColor::fromRgba(color);
            draw(painter, center, own);
            continue;
        }
        const double coarse = sizes.empty() ? size : coarseSize(size);
        const QRgb   shade  = colors.empty() ? color : coarseColor(color);
        if (coarse != spriteSize || shade != spriteColor)
        {
            own.size    = coarse;
            own.color   = QColor::fromRgba(shade);
            pixmap      = sprite(own, dpr);
            spriteSize  = coarse;
            spriteColor = shade;
        }
        const double half = static_cast<double>(pixmap.width()) / dpr / 2.0;
        painter.drawPixmap(QPointF(std::round((center.x() - half) * dpr) / dpr,
                                   std::round((center.y() - half) * dpr) / dpr),
                           pixmap);
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
    const SpriteKey key{
        .shape            = style.shape,
        .size             = style.size,
        .color            = style.color.rgba(),
        .ring             = style.ring.rgba(),
        .ringWidth        = style.ringWidth,
        .devicePixelRatio = devicePixelRatio,
    };
    if (const auto found = m_sprites.constFind(key); found != m_sprites.constEnd())
    {
        return *found;
    }

    // Room for the shape, its ring and antialiasing, in whole device pixels, odd so the center is a
    // pixel center.
    const double extent     = style.size + (2.0 * style.ringWidth) +
                              (isStroked(style.shape) ? strokeWidth(style) : 0.0) + 2.0;
    int          deviceSize = static_cast<int>(std::ceil(extent * devicePixelRatio));
    deviceSize += deviceSize % 2 == 0 ? 1 : 0;

    const qsizetype bytes = qsizetype{4} * deviceSize * deviceSize;
    if (m_sprites.size() >= kMaxSprites || m_spriteBytes + bytes > kMaxSpriteBytes)
    {
        m_sprites.clear();
        m_spriteBytes = 0;
    }
    m_spriteBytes += bytes;

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
