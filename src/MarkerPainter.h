#pragma once

#include <QColor>
#include <QHash>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QRgb>
#include <cstddef>
#include <span>

#include "core/Decimator.h"
#include "rocketplot/enums.h"

class QPainter;

namespace rocketplot
{

class Series;
struct Theme;

/// How markers look: a shape filled (or, for CROSS and PLUS, stroked) in the series color, inside a
/// ring of the background color that keeps overlapping markers and crossing lines apart.
struct MarkerStyle
{
    Marker shape = Marker::CIRCLE;
    double size  = 8.0;
    QColor color;
    QColor ring;
    double ringWidth = 2.0;
};

/// How @p series draws its markers with @p theme.
[[nodiscard]] MarkerStyle markerStyle(const Series& series, const Theme& theme);

/// The outline of @p marker with diameter @p size, centered on (0, 0). Empty for Marker::NONE.
[[nodiscard]] QPainterPath markerPath(Marker marker, double size);

/// Draws markers. On a raster device (the screen) each marker is a copy of a cached pre-rendered
/// pixmap, which is much faster than filling paths when there are many; on vector devices (SVG,
/// PDF) they are paths.
class MarkerPainter
{
public:
    void draw(QPainter& painter, std::span<const core::PixelPoint> centers,
              const MarkerStyle& style);
    /// Draws markers that have their own @p sizes and @p colors (one per center; an empty span
    /// means @p style's for all of them). On a raster device, sizes and colors are rounded to steps
    /// too small to see, so that markers share pixmaps.
    void        draw(QPainter& painter, std::span<const core::PixelPoint> centers,
                     std::span<const double> sizes, std::span<const QRgb> colors,
                     const MarkerStyle& style);
    static void draw(QPainter& painter, QPointF center, const MarkerStyle& style);

private:
    // What a pre-rendered marker looks like.
    struct SpriteKey
    {
        Marker shape            = Marker::NONE;
        double size             = 0.0;
        QRgb   color            = 0;
        QRgb   ring             = 0;
        double ringWidth        = 0.0;
        double devicePixelRatio = 1.0;

        friend bool   operator==(const SpriteKey&, const SpriteKey&) = default;
        friend size_t qHash(const SpriteKey& key, size_t seed = 0) noexcept
        {
            return qHashMulti(seed, static_cast<int>(key.shape), key.size, key.color, key.ring,
                              key.ringWidth, key.devicePixelRatio);
        }
    };

    [[nodiscard]] const QPixmap& sprite(const MarkerStyle& style, double devicePixelRatio);

    QHash<SpriteKey, QPixmap> m_sprites;
    qsizetype                 m_spriteBytes = 0;  // of the sprites' pixels
};

}  // namespace rocketplot
