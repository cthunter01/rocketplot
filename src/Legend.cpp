#include "rocketplot/Legend.h"

#include <QObject>
#include <QPointF>
#include <algorithm>
#include <cmath>

#include "rocketplot/enums.h"

namespace rocketplot
{

Legend::Legend(QObject* parent) : QObject(parent) { }

Legend::~Legend() = default;

void Legend::setVisible(bool visible)
{
    if (m_visible == visible)
    {
        return;
    }
    m_visible = visible;
    Q_EMIT changed();
}

void Legend::resetVisible()
{
    if (!m_visible)
    {
        return;
    }
    m_visible.reset();
    Q_EMIT changed();
}

bool Legend::isShownFor(qsizetype entryCount) const noexcept
{
    if (m_visible)
    {
        return *m_visible && entryCount > 0;
    }
    return entryCount >= 2;
}

void Legend::setAnchor(LegendAnchor anchor)
{
    if (anchor == m_anchor)
    {
        return;
    }
    m_anchor = anchor;
    Q_EMIT changed();
}

void Legend::setPosition(QPointF position)
{
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()))
    {
        return;
    }
    position = QPointF(std::clamp(position.x(), 0.0, 1.0), std::clamp(position.y(), 0.0, 1.0));
    if (position == m_position && m_anchor == LegendAnchor::CUSTOM)
    {
        return;
    }
    m_position = position;
    m_anchor   = LegendAnchor::CUSTOM;
    Q_EMIT changed();
}

void Legend::setInteractive(bool interactive)
{
    if (interactive == m_interactive)
    {
        return;
    }
    m_interactive = interactive;
    Q_EMIT changed();
}

void Legend::setValuesVisible(bool visible)
{
    if (visible == m_valuesVisible)
    {
        return;
    }
    m_valuesVisible = visible;
    Q_EMIT changed();
}

}  // namespace rocketplot
