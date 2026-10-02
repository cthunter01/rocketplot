#pragma once

#include <QColor>
#include <QHash>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <span>

#include "core/Decimator.h"
#include "rocketplot/enums.h"

class QPainter;

namespace rocketplot
{

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

/// The outline of @p marker with diameter @p size, centered on (0, 0). Empty for Marker::NONE.
[[nodiscard]] QPainterPath markerPath(Marker marker, double size);

/// Draws markers. On a raster device (the screen) each marker is a copy of a cached pre-rendered
/// pixmap, which is much faster than filling paths when there are many; on vector devices (SVG,
/// PDF) they are paths.
class MarkerPainter
{
public:
    void        draw(QPainter& painter, std::span<const core::PixelPoint> centers,
                     const MarkerStyle& style);
    static void draw(QPainter& painter, QPointF center, const MarkerStyle& style);

private:
    [[nodiscard]] const QPixmap& sprite(const MarkerStyle& style, double devicePixelRatio);

    QHash<QString, QPixmap> m_sprites;
};

}  // namespace rocketplot
