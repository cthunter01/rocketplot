#include "rocketplot/ShadedSpan.h"

#include <QString>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <utility>

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

Range ordered(Range range)
{
    if (range.min > range.max)
    {
        std::swap(range.min, range.max);
    }
    return range;
}

}  // namespace

ShadedSpan::ShadedSpan(PlotWidget* plot, Qt::Orientation orientation, Range range)
  : Annotation(plot, AnnotationLayer::BELOW_SERIES),
    m_orientation(orientation),
    m_range(ordered(range))
{
}

ShadedSpan::~ShadedSpan() = default;

void ShadedSpan::setRange(Range range)
{
    range = ordered(range);
    // Compared end by end: a NaN end never equals itself, and the range is then set again.
    if (range.min == m_range.min && range.max == m_range.max)
    {
        return;
    }
    m_range = range;
    Q_EMIT changed();
}

void ShadedSpan::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

double ShadedSpan::opacity() const
{
    return m_opacity.value_or(plot()->theme().spanOpacity);
}

void ShadedSpan::setOpacity(double opacity)
{
    if (!std::isfinite(opacity))
    {
        qCWarning(lcData) << "ShadedSpan::setOpacity: ignoring" << opacity;
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (m_opacity == opacity)
    {
        return;
    }
    m_opacity = opacity;
    Q_EMIT changed();
}

void ShadedSpan::resetOpacity()
{
    if (!m_opacity)
    {
        return;
    }
    m_opacity.reset();
    Q_EMIT changed();
}

void ShadedSpan::setLabelAlignment(Qt::Alignment alignment)
{
    if (alignment == m_labelAlignment)
    {
        return;
    }
    m_labelAlignment = alignment;
    Q_EMIT changed();
}

Range ShadedSpan::xExtent() const
{
    // Infinite ends are left out: autoscale fits the finite ones.
    return m_orientation == Qt::Vertical
               ? Range::empty().including(m_range.min).including(m_range.max)
               : Range::empty();
}

Range ShadedSpan::yExtent() const
{
    return m_orientation == Qt::Horizontal
               ? Range::empty().including(m_range.min).including(m_range.max)
               : Range::empty();
}

}  // namespace rocketplot
