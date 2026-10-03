#include "PlotExport.h"

#include <QImage>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QSvgGenerator>
#include <Qt>
#include <cmath>

namespace rocketplot
{

namespace
{

// The resolution of an image with one pixel per device-independent pixel.
constexpr double kBaseDpi        = 96.0;
constexpr double kPointsPerInch  = 72.0;
constexpr double kMetersPerInch  = 0.0254;
constexpr int    kMaxImageExtent = 32767;  // QImage's limit

QRectF boundsOf(QSize size)
{
    return {QPointF(), QSizeF(size)};
}

}  // namespace

QImage paintImage(QSize size, double pixelRatio, const PlotPainter& paint)
{
    const QSize pixels(static_cast<int>(std::ceil(size.width() * pixelRatio)),
                       static_cast<int>(std::ceil(size.height() * pixelRatio)));
    if (size.isEmpty() || !(pixelRatio > 0.0) || pixels.width() > kMaxImageExtent ||
        pixels.height() > kMaxImageExtent)
    {
        return {};
    }
    QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
    {
        return image;  // not enough memory
    }
    image.setDevicePixelRatio(pixelRatio);
    image.fill(Qt::transparent);  // what shows through a theme whose background isn't opaque
    {
        QPainter painter(&image);
        paint(painter, boundsOf(size), pixelRatio);
    }
    // Only now: while painting, the image's resolution is the screen's, which keeps text sized in
    // points the size it is on screen (the pixel ratio does the scaling).
    const int dotsPerMeter = static_cast<int>(std::lround(kBaseDpi * pixelRatio / kMetersPerInch));
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);
    return image;
}

bool writeSvg(const QString& fileName, QSize size, int logicalDpi, const QString& title,
              const PlotPainter& paint)
{
    if (size.isEmpty())
    {
        return false;
    }
    // SVG 1.1 rather than the default Tiny 1.2, which has no clipping: the series are clipped to
    // the plot area.
    QSvgGenerator generator(QSvgGenerator::SvgVersion::Svg11);
    generator.setFileName(fileName);
    generator.setSize(size);
    generator.setViewBox(QRect(QPoint(), size));
    generator.setResolution(logicalDpi);
    generator.setTitle(title);
    QPainter painter;
    if (!painter.begin(&generator))
    {
        return false;
    }
    paint(painter, boundsOf(size), 1.0);
    return painter.end();
}

bool writePdf(const QString& fileName, QSize size, int logicalDpi, const QString& title,
              const PlotPainter& paint)
{
    if (size.isEmpty() || logicalDpi <= 0)
    {
        return false;
    }
    QPdfWriter writer(fileName);
    // One unit of the painter is one device-independent pixel, and the page is the plot.
    writer.setResolution(logicalDpi);
    writer.setPageSize(QPageSize(QSizeF(size) * (kPointsPerInch / logicalDpi), QPageSize::Point,
                                 QString(), QPageSize::ExactMatch));
    writer.setPageMargins(QMarginsF());
    writer.setTitle(title);
    writer.setCreator(QStringLiteral("rocketplot"));
    QPainter painter;
    if (!painter.begin(&writer))
    {
        return false;
    }
    paint(painter, boundsOf(size), 1.0);
    return painter.end();
}

}  // namespace rocketplot
