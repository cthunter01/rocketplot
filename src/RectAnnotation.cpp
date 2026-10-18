#include "rocketplot/RectAnnotation.h"

#include <QRectF>
#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>

#include "Logging.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// Whether two rectangles are the same, edge by edge (QRectF's == is fuzzy, and false for NaN).
bool same(const QRectF& a, const QRectF& b)
{
    return a.left() == b.left() && a.top() == b.top() && a.right() == b.right() &&
           a.bottom() == b.bottom();
}

}  // namespace

RectAnnotation::RectAnnotation(PlotWidget* plot, const QRectF& rect)
  : Annotation(plot, AnnotationLayer::BELOW_SERIES), m_rect(rect.normalized())
{
}

RectAnnotation::~RectAnnotation() = default;

void RectAnnotation::setRect(const QRectF& rect)
{
    const QRectF normalized = rect.normalized();
    if (same(normalized, m_rect))
    {
        return;
    }
    m_rect = normalized;
    Q_EMIT changed();
}

Range RectAnnotation::xRange() const noexcept
{
    return {.min = m_rect.left(), .max = m_rect.right()};
}

Range RectAnnotation::yRange() const noexcept
{
    return {.min = m_rect.top(), .max = m_rect.bottom()};
}

void RectAnnotation::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

double RectAnnotation::lineWidth() const
{
    return m_lineWidth.value_or(plot()->theme().annotationLineWidth);
}

void RectAnnotation::setLineWidth(double width)
{
    if (!std::isfinite(width) || width <= 0.0)
    {
        qCWarning(lcData) << "RectAnnotation::setLineWidth: ignoring" << width;
        return;
    }
    if (m_lineWidth == width)
    {
        return;
    }
    m_lineWidth = width;
    Q_EMIT changed();
}

void RectAnnotation::resetLineWidth()
{
    if (!m_lineWidth)
    {
        return;
    }
    m_lineWidth.reset();
    Q_EMIT changed();
}

void RectAnnotation::setLineStyle(Qt::PenStyle style)
{
    if (style == m_lineStyle)
    {
        return;
    }
    m_lineStyle = style;
    Q_EMIT changed();
}

void RectAnnotation::setFillOpacity(double opacity)
{
    if (!std::isfinite(opacity))
    {
        qCWarning(lcData) << "RectAnnotation::setFillOpacity: ignoring" << opacity;
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (opacity == m_fillOpacity)
    {
        return;
    }
    m_fillOpacity = opacity;
    Q_EMIT changed();
}

void RectAnnotation::setLabelAlignment(Qt::Alignment alignment)
{
    if (alignment == m_labelAlignment)
    {
        return;
    }
    m_labelAlignment = alignment;
    Q_EMIT changed();
}

Range RectAnnotation::xExtent() const
{
    // Edges that aren't finite are left out.
    return Range::empty().including(m_rect.left()).including(m_rect.right());
}

Range RectAnnotation::yExtent() const
{
    return Range::empty().including(m_rect.top()).including(m_rect.bottom());
}

}  // namespace rocketplot
