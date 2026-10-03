#include "rocketplot/ReferenceLine.h"

#include <QString>
#include <Qt>
#include <cmath>

#include "Logging.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

ReferenceLine::ReferenceLine(PlotWidget* plot, Qt::Orientation orientation, double value)
  : Annotation(plot, AnnotationLayer::ABOVE_SERIES), m_orientation(orientation), m_value(value)
{
}

ReferenceLine::~ReferenceLine() = default;

void ReferenceLine::setValue(double value)
{
    if (value == m_value)
    {
        return;
    }
    m_value = value;
    Q_EMIT changed();
}

void ReferenceLine::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

double ReferenceLine::lineWidth() const
{
    return m_lineWidth.value_or(plot()->theme().annotationLineWidth);
}

void ReferenceLine::setLineWidth(double width)
{
    if (!std::isfinite(width) || width <= 0.0)
    {
        qCWarning(lcData) << "ReferenceLine::setLineWidth: ignoring" << width;
        return;
    }
    if (m_lineWidth == width)
    {
        return;
    }
    m_lineWidth = width;
    Q_EMIT changed();
}

void ReferenceLine::resetLineWidth()
{
    if (!m_lineWidth)
    {
        return;
    }
    m_lineWidth.reset();
    Q_EMIT changed();
}

void ReferenceLine::setLineStyle(Qt::PenStyle style)
{
    if (style == m_lineStyle)
    {
        return;
    }
    m_lineStyle = style;
    Q_EMIT changed();
}

void ReferenceLine::setLabelAlignment(Qt::Alignment alignment)
{
    if (alignment == m_labelAlignment)
    {
        return;
    }
    m_labelAlignment = alignment;
    Q_EMIT changed();
}

Range ReferenceLine::xExtent() const
{
    return m_orientation == Qt::Vertical ? Range::empty().including(m_value) : Range::empty();
}

Range ReferenceLine::yExtent() const
{
    return m_orientation == Qt::Horizontal ? Range::empty().including(m_value) : Range::empty();
}

}  // namespace rocketplot
