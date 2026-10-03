#include "rocketplot/TextAnnotation.h"

#include <QColor>
#include <QPointF>
#include <QString>
#include <Qt>
#include <utility>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

TextAnnotation::TextAnnotation(PlotWidget* plot, QPointF position, QString text)
  : Annotation(plot, AnnotationLayer::ABOVE_SERIES), m_position(position), m_text(std::move(text))
{
}

TextAnnotation::~TextAnnotation() = default;

void TextAnnotation::setPosition(QPointF position)
{
    if (position == m_position)
    {
        return;
    }
    m_position = position;
    Q_EMIT changed();
}

void TextAnnotation::setText(const QString& text)
{
    if (text == m_text)
    {
        return;
    }
    m_text = text;
    Q_EMIT changed();
}

void TextAnnotation::setOffset(QPointF offset)
{
    if (offset == m_offset)
    {
        return;
    }
    m_offset = offset;
    Q_EMIT changed();
}

void TextAnnotation::setAlignment(Qt::Alignment alignment)
{
    if (alignment == m_alignment)
    {
        return;
    }
    m_alignment = alignment;
    Q_EMIT changed();
}

void TextAnnotation::setArrowVisible(bool visible)
{
    if (visible == m_arrowVisible)
    {
        return;
    }
    m_arrowVisible = visible;
    Q_EMIT changed();
}

void TextAnnotation::setBackgroundVisible(bool visible)
{
    if (visible == m_backgroundVisible)
    {
        return;
    }
    m_backgroundVisible = visible;
    Q_EMIT changed();
}

QColor TextAnnotation::themeColor(const Theme& theme) const
{
    return theme.text;
}

Range TextAnnotation::xExtent() const
{
    return Range::empty().including(m_position.x());
}

Range TextAnnotation::yExtent() const
{
    return Range::empty().including(m_position.y());
}

}  // namespace rocketplot
