#include "rocketplot/Annotation.h"

#include <QColor>
#include <QObject>

#include "Logging.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

Annotation::Annotation(PlotWidget* plot, AnnotationLayer layer)
  : QObject(plot), m_plot(plot), m_layer(layer)
{
}

Annotation::~Annotation() = default;

void Annotation::setVisible(bool visible)
{
    if (visible == m_visible)
    {
        return;
    }
    m_visible = visible;
    Q_EMIT changed();
}

QColor Annotation::color() const
{
    return m_color ? *m_color : themeColor(m_plot->theme());
}

void Annotation::setColor(const QColor& color)
{
    if (!color.isValid())
    {
        resetColor();
        return;
    }
    if (m_color == color)
    {
        return;
    }
    m_color = color;
    Q_EMIT changed();
}

void Annotation::resetColor()
{
    if (!m_color)
    {
        return;
    }
    m_color.reset();
    Q_EMIT changed();
}

void Annotation::setLayer(AnnotationLayer layer)
{
    if (layer == m_layer)
    {
        return;
    }
    m_layer = layer;
    Q_EMIT changed();
}

Axis* Annotation::yAxis() const
{
    return m_secondaryYAxis ? m_plot->yAxis2() : m_plot->yAxis();
}

void Annotation::setYAxis(Axis* axis)
{
    if (axis != m_plot->yAxis() && axis != m_plot->yAxis2())
    {
        qCWarning(lcData) << "Annotation::setYAxis: not a y axis of this annotation's plot";
        return;
    }
    setOnSecondaryYAxis(axis == m_plot->yAxis2());
}

void Annotation::setOnSecondaryYAxis(bool secondary)
{
    if (secondary == m_secondaryYAxis)
    {
        return;
    }
    m_secondaryYAxis = secondary;
    Q_EMIT changed();
}

void Annotation::setIncludedInAutoscale(bool included)
{
    if (included == m_includedInAutoscale)
    {
        return;
    }
    m_includedInAutoscale = included;
    Q_EMIT changed();
}

QColor Annotation::themeColor(const Theme& theme) const
{
    return theme.annotation;
}

Range Annotation::xExtent() const
{
    return Range::empty();
}

Range Annotation::yExtent() const
{
    return Range::empty();
}

}  // namespace rocketplot
