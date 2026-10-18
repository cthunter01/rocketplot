#include "rocketplot/IconAnnotation.h"

#include <QIcon>
#include <QPointF>
#include <QSizeF>
#include <Qt>
#include <cmath>
#include <utility>

#include "Logging.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

constexpr QSizeF kDefaultSize{16.0, 16.0};

}  // namespace

IconAnnotation::IconAnnotation(PlotWidget* plot, QPointF position, QIcon icon)
  : Annotation(plot, AnnotationLayer::ABOVE_SERIES), m_position(position), m_icon(std::move(icon))
{
}

IconAnnotation::~IconAnnotation() = default;

void IconAnnotation::setPosition(QPointF position)
{
    if (position == m_position)
    {
        return;
    }
    m_position = position;
    Q_EMIT changed();
}

void IconAnnotation::setIcon(const QIcon& icon)
{
    if ((icon.isNull() && m_icon.isNull()) ||
        (!icon.isNull() && !m_icon.isNull() && icon.cacheKey() == m_icon.cacheKey()))
    {
        return;
    }
    m_icon = icon;
    Q_EMIT changed();
}

QSizeF IconAnnotation::size() const noexcept
{
    return m_size.value_or(kDefaultSize);
}

void IconAnnotation::setSize(QSizeF size)
{
    if (!std::isfinite(size.width()) || !std::isfinite(size.height()) || !(size.width() > 0.0) ||
        !(size.height() > 0.0))
    {
        qCWarning(lcData) << "IconAnnotation::setSize: ignoring" << size;
        return;
    }
    if (m_size == size)
    {
        return;
    }
    m_size = size;
    Q_EMIT changed();
}

void IconAnnotation::resetSize()
{
    if (!m_size)
    {
        return;
    }
    m_size.reset();
    Q_EMIT changed();
}

void IconAnnotation::setAlignment(Qt::Alignment alignment)
{
    if (alignment == m_alignment)
    {
        return;
    }
    m_alignment = alignment;
    Q_EMIT changed();
}

void IconAnnotation::setOffset(QPointF offset)
{
    if (offset == m_offset)
    {
        return;
    }
    m_offset = offset;
    Q_EMIT changed();
}

Range IconAnnotation::xExtent() const
{
    return Range::empty().including(m_position.x());
}

Range IconAnnotation::yExtent() const
{
    return Range::empty().including(m_position.y());
}

}  // namespace rocketplot
