#include "rocketplot/LineSeries.h"

#include <QColor>
#include <QPen>
#include <QSignalBlocker>
#include <Qt>
#include <cmath>

#include "Logging.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

LineSeries::LineSeries(PlotWidget* plot) : Series(plot, Marker::NONE) { }

LineSeries::~LineSeries() = default;

double LineSeries::lineWidth() const
{
    return m_lineWidth.value_or(plot()->theme().lineWidth);
}

void LineSeries::setLineWidth(double width)
{
    if (!std::isfinite(width) || width <= 0.0)
    {
        qCWarning(lcData) << "LineSeries::setLineWidth: ignoring" << width;
        return;
    }
    if (m_lineWidth == width)
    {
        return;
    }
    m_lineWidth = width;
    Q_EMIT changed();
}

void LineSeries::resetLineWidth()
{
    if (!m_lineWidth)
    {
        return;
    }
    m_lineWidth.reset();
    Q_EMIT changed();
}

void LineSeries::setLineStyle(Qt::PenStyle style)
{
    if (style == m_lineStyle)
    {
        return;
    }
    m_lineStyle = style;
    Q_EMIT changed();
}

QPen LineSeries::pen() const
{
    return {color(), lineWidth(), m_lineStyle, Qt::RoundCap, Qt::RoundJoin};
}

void LineSeries::setPen(const QPen& pen)
{
    // One changed() for the lot: block the setters' signals.
    {
        const QSignalBlocker blocker(this);
        setColor(pen.color());
        if (pen.widthF() > 0.0)  // QPen's 0 means a cosmetic 1-pixel line
        {
            setLineWidth(pen.widthF());
        }
        setLineStyle(pen.style());
    }
    Q_EMIT changed();
}

}  // namespace rocketplot
