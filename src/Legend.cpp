#include "rocketplot/Legend.h"

#include <QObject>

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

}  // namespace rocketplot
