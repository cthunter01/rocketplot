#include "TextPainter.h"

#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <Qt>
#include <memory>
#include <utility>

namespace rocketplot
{

namespace
{

// Rich text documents kept; the cache starts over beyond this (labels rarely change).
constexpr qsizetype kMaxDocuments = 64;

// The top left of a box of @p size aligned in @p box.
QPointF alignedTopLeft(const QSizeF& size, const QRectF& box, Qt::Alignment alignment)
{
    double x = box.left();
    if (alignment.testFlag(Qt::AlignHCenter))
    {
        x = box.center().x() - (size.width() / 2.0);
    }
    else if (alignment.testFlag(Qt::AlignRight))
    {
        x = box.right() - size.width();
    }
    double y = box.top();
    if (alignment.testFlag(Qt::AlignVCenter))
    {
        y = box.center().y() - (size.height() / 2.0);
    }
    else if (alignment.testFlag(Qt::AlignBottom))
    {
        y = box.bottom() - size.height();
    }
    return {x, y};
}

}  // namespace

QString plainText(const QString& text)
{
    return Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText() : text;
}

TextPainter::TextPainter()  = default;
TextPainter::~TextPainter() = default;

QTextDocument& TextPainter::document(const QString& text, const QFont& font)
{
    const QString key = font.key() + QLatin1Char('\n') + text;
    if (const auto found = m_documents.constFind(key); found != m_documents.constEnd())
    {
        return **found;
    }
    if (m_documents.size() >= kMaxDocuments)
    {
        m_documents.clear();
    }
    auto document = std::make_shared<QTextDocument>();
    document->setDocumentMargin(0.0);
    document->setDefaultFont(font);
    document->setHtml(text);
    return **m_documents.insert(key, std::move(document));
}

QSizeF TextPainter::size(const QString& text, const QFont& font)
{
    if (text.isEmpty())
    {
        return {};
    }
    if (Qt::mightBeRichText(text))
    {
        return document(text, font).size();
    }
    const QFontMetricsF metrics(font);
    return {metrics.horizontalAdvance(text), metrics.height()};
}

void TextPainter::draw(QPainter& painter, const QString& text, const QFont& font,
                       const QColor& color, const QRectF& box, Qt::Alignment alignment, bool elide)
{
    if (text.isEmpty())
    {
        return;
    }
    if (!Qt::mightBeRichText(text))
    {
        painter.setFont(font);
        painter.setPen(color);
        const QString shown =
            elide ? QFontMetricsF(font).elidedText(text, Qt::ElideRight, box.width()) : text;
        painter.drawText(box, static_cast<int>(alignment.toInt()), shown);
        return;
    }
    const QTextDocument& rich = document(text, font);
    painter.save();
    painter.translate(alignedTopLeft(rich.size(), box, alignment));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, color);
    rich.documentLayout()->draw(&painter, context);
    painter.restore();
}

}  // namespace rocketplot
