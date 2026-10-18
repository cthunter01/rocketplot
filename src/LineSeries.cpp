#include "rocketplot/LineSeries.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QList>
#include <QPen>
#include <QSignalBlocker>
#include <Qt>
#include <algorithm>
#include <cmath>

#include "Logging.h"
#include "PlotState.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

LineSeries::LineSeries(PlotWidget* plot) : Series(plot, Marker::NONE, ErrorStyle::BAND) { }

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

void LineSeries::setDashPattern(const QList<qreal>& pattern)
{
    if (pattern.size() % 2 != 0 || std::ranges::any_of(pattern, [](qreal length) {
            return !(length > 0.0) || !std::isfinite(length);
        }))
    {
        qCWarning(lcData) << "LineSeries::setDashPattern: ignoring" << pattern;
        return;
    }
    if (pattern == m_dashPattern)
    {
        return;
    }
    m_dashPattern = pattern;
    Q_EMIT changed();
}

Qt::PenCapStyle LineSeries::capStyle() const noexcept
{
    return m_capStyle.value_or(Qt::RoundCap);
}

void LineSeries::setCapStyle(Qt::PenCapStyle style)
{
    if (m_capStyle == style)
    {
        return;
    }
    m_capStyle = style;
    Q_EMIT changed();
}

void LineSeries::resetCapStyle()
{
    if (!m_capStyle)
    {
        return;
    }
    m_capStyle.reset();
    Q_EMIT changed();
}

void LineSeries::setMarkersAlwaysVisible(bool visible)
{
    if (visible == m_markersAlwaysVisible)
    {
        return;
    }
    m_markersAlwaysVisible = visible;
    Q_EMIT changed();
}

QPen LineSeries::pen() const
{
    const double width = lineWidth();
    QPen         pen(color(), width, m_lineStyle, capStyle(), Qt::RoundJoin);
    if (!m_dashPattern.isEmpty())
    {
        // QPen measures dashes in line widths.
        QList<qreal> dashes;
        dashes.reserve(m_dashPattern.size());
        for (const qreal length : m_dashPattern)
        {
            dashes.append(length / width);
        }
        pen.setDashPattern(dashes);
    }
    return pen;
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
        if (pen.style() == Qt::CustomDashLine)
        {
            QList<qreal> lengths;
            for (const qreal dash : pen.dashPattern())
            {
                lengths.append(dash * lineWidth());
            }
            setDashPattern(lengths);
        }
        else
        {
            setDashPattern({});
            setLineStyle(pen.style());
        }
        setCapStyle(pen.capStyle());
    }
    Q_EMIT changed();
}

void LineSeries::saveState(QJsonObject& state) const
{
    Series::saveState(state);
    state.insert(QLatin1String("lineWidth"), state::fromOptional(m_lineWidth));
    state.insert(QLatin1String("lineStyle"), state::fromEnum(m_lineStyle));
    QJsonArray dashes;
    for (const qreal length : m_dashPattern)
    {
        dashes.append(length);
    }
    state.insert(QLatin1String("dashPattern"), dashes);
    state.insert(QLatin1String("capStyle"),
                 m_capStyle ? state::fromEnum(*m_capStyle) : QJsonValue(QJsonValue::Null));
    state.insert(QLatin1String("markersAlwaysVisible"), m_markersAlwaysVisible);
}

void LineSeries::restoreState(const QJsonObject& state)
{
    Series::restoreState(state);
    state::restoreOptional(
        state.value(QLatin1String("lineWidth")), state::toNumber,
        [this](double width) { setLineWidth(width); }, [this] { resetLineWidth(); });
    if (const auto style = state::toEnum<Qt::PenStyle>(state.value(QLatin1String("lineStyle"))))
    {
        setLineStyle(*style);
    }
    if (const QJsonValue dashes = state.value(QLatin1String("dashPattern")); dashes.isArray())
    {
        QList<qreal> pattern;
        for (const QJsonValue length : dashes.toArray())
        {
            if (const auto number = state::toNumber(length))
            {
                pattern.append(*number);
            }
        }
        setDashPattern(pattern);
    }
    state::restoreOptional(
        state.value(QLatin1String("capStyle")), state::toEnum<Qt::PenCapStyle>,
        [this](Qt::PenCapStyle style) { setCapStyle(style); }, [this] { resetCapStyle(); });
    if (const auto always = state::toBool(state.value(QLatin1String("markersAlwaysVisible"))))
    {
        setMarkersAlwaysVisible(*always);
    }
}

}  // namespace rocketplot
