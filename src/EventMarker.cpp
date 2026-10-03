#include "rocketplot/EventMarker.h"

#include <QString>
#include <utility>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

EventMarker::EventMarker(PlotWidget* plot, double x, QString label)
  : Annotation(plot, AnnotationLayer::ABOVE_SERIES), m_x(x), m_label(std::move(label))
{
}

EventMarker::~EventMarker() = default;

void EventMarker::setX(double x)
{
    if (x == m_x)
    {
        return;
    }
    m_x = x;
    Q_EMIT changed();
}

void EventMarker::setLabel(const QString& label)
{
    if (label == m_label)
    {
        return;
    }
    m_label = label;
    Q_EMIT changed();
}

Range EventMarker::xExtent() const
{
    return Range::empty().including(m_x);
}

}  // namespace rocketplot
