#include "TextPainter.h"

#include <QAbstractTextDocumentLayout>
#include <QChar>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <Qt>
#include <memory>
#include <utility>

#include "MathTextObject.h"
#include "core/MathParser.h"
#include "core/MathPlainText.h"
#include "core/MathSegments.h"

namespace rocketplot
{

namespace
{

// Rich text documents kept; the cache starts over beyond this (labels rarely change).
constexpr qsizetype kMaxDocuments = 64;

// A formula's place in a label's text while the text is made into a document: a character of the
// private use area, which no label has (the formula itself might look like HTML: "a<b").
constexpr char16_t  kFirstPlaceholder = 0xE000;
constexpr qsizetype kMaxFormulas      = 0x1000;

QString placeholder(qsizetype formula)
{
    return {QChar(static_cast<char16_t>(kFirstPlaceholder + formula))};
}

// A label's text with a placeholder where each formula goes, and the formulas.
struct MaskedText
{
    QString     text;
    QStringList formulas;
};

MaskedText masked(const QString& label)
{
    MaskedText result;
    for (const core::MathSegment& segment : core::splitMath(label.toStdString()))
    {
        const QString part = QString::fromStdString(segment.text);
        if (!segment.math)
        {
            result.text += part;
        }
        else if (result.formulas.size() < kMaxFormulas)
        {
            result.text += placeholder(result.formulas.size());
            result.formulas.append(part);
        }
        else
        {
            result.text += u'$' + part + u'$';
        }
    }
    return result;
}

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

// @p formula as plain text ("v_z", "F/m"), or as typed if it can't be parsed.
QString plainFormula(const QString& formula)
{
    const auto list = core::parseMath(formula.toStdString());
    return list ? QString::fromStdString(core::mathToPlainText(*list)) : u'$' + formula + u'$';
}

}  // namespace

QString plainText(const QString& text)
{
    if (!text.contains(u'$'))
    {
        return Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText()
                                         : text;
    }
    const MaskedText label = masked(text);
    QString          plain = Qt::mightBeRichText(label.text)
                                 ? QTextDocumentFragment::fromHtml(label.text).toPlainText()
                                 : label.text;
    for (qsizetype i = 0; i < label.formulas.size(); ++i)
    {
        plain.replace(placeholder(i), plainFormula(label.formulas.at(i)));
    }
    return plain;
}

TextPainter::TextPainter() : m_mathObjects(std::make_unique<MathTextObject>(m_math)) { }

TextPainter::~TextPainter() = default;

void TextPainter::setMathFontFamily(const QString& family)
{
    if (family == m_math.fontFamily())
    {
        return;
    }
    m_math.setFontFamily(family);
    m_documents.clear();  // they measured the formulas in the other font
}

QTextDocument* TextPainter::document(const QString& text, const QFont& font)
{
    const QString key = font.key() + QLatin1Char('\n') + text;
    if (const auto found = m_documents.constFind(key); found != m_documents.constEnd())
    {
        return found->get();
    }
    const MaskedText label =
        text.contains(u'$') ? masked(text) : MaskedText{.text = text, .formulas = {}};
    if (label.formulas.isEmpty() && !Qt::mightBeRichText(label.text))
    {
        return nullptr;
    }

    auto document = std::make_shared<QTextDocument>();
    document->setUndoRedoEnabled(false);
    document->setDocumentMargin(0.0);
    document->setDefaultFont(font);
    document->documentLayout()->registerHandler(MathTextObject::kObjectType, m_mathObjects.get());
    if (Qt::mightBeRichText(label.text))
    {
        document->setHtml(label.text);
    }
    else
    {
        document->setPlainText(label.text);
    }
    // Each formula's placeholder becomes its object, in the format of the text around it.
    for (qsizetype i = 0; i < label.formulas.size(); ++i)
    {
        QTextCursor cursor = document->find(placeholder(i));
        if (cursor.isNull())
        {
            continue;
        }
        const QTextCharFormat around  = cursor.charFormat();
        const QString&        formula = label.formulas.at(i);
        if (const auto object = m_mathObjects->format(formula, around, font))
        {
            cursor.insertText(QString(QChar::ObjectReplacementCharacter), *object);
        }
        else if (!m_math.hasFont())
        {
            cursor.insertText(plainFormula(formula), around);  // nothing to set it in
        }
        else
        {
            cursor.insertText(u'$' + formula + u'$', around);  // as typed
        }
    }

    if (m_documents.size() >= kMaxDocuments)
    {
        m_documents.clear();
    }
    return m_documents.insert(key, std::move(document))->get();
}

QSizeF TextPainter::size(const QString& text, const QFont& font)
{
    if (text.isEmpty())
    {
        return {};
    }
    if (const QTextDocument* rich = document(text, font))
    {
        return rich->size();
    }
    const QString       shown = text.contains(u'$') ? masked(text).text : text;
    const QFontMetricsF metrics(font);
    return {metrics.horizontalAdvance(shown), metrics.height()};
}

void TextPainter::draw(QPainter& painter, const QString& text, const QFont& font,
                       const QColor& color, const QRectF& box, Qt::Alignment alignment, bool elide)
{
    if (text.isEmpty())
    {
        return;
    }
    const QTextDocument* rich = document(text, font);
    if (rich == nullptr)
    {
        // Without formulas, all a $ can be is a price: as typed, less the backslash of a \$.
        const QString shown = text.contains(u'$') ? masked(text).text : text;
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText(
            box, static_cast<int>(alignment.toInt()),
            elide ? QFontMetricsF(font).elidedText(shown, Qt::ElideRight, box.width()) : shown);
        return;
    }
    painter.save();
    painter.translate(alignedTopLeft(rich->size(), box, alignment));
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, color);
    rich->documentLayout()->draw(&painter, context);
    painter.restore();
}

}  // namespace rocketplot
