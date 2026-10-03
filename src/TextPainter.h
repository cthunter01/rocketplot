#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <memory>

class QPainter;
class QTextDocument;

namespace rocketplot
{

/// @p text without its markup, if it is Qt rich text (for a file's title, a CSV header).
[[nodiscard]] QString plainText(const QString& text);

/// Measures and draws a plot's titles, axis labels and legend names. Plain strings are drawn
/// directly; Qt rich text (Qt::mightBeRichText(): "v<sub>z</sub>", "<i>T</i><sub>0</sub>") goes
/// through a QTextDocument, cached per text and font.
class TextPainter
{
public:
    TextPainter();
    ~TextPainter();
    TextPainter(const TextPainter&)            = delete;
    TextPainter& operator=(const TextPainter&) = delete;
    TextPainter(TextPainter&&)                 = delete;
    TextPainter& operator=(TextPainter&&)      = delete;

    [[nodiscard]] QSizeF size(const QString& text, const QFont& font);
    /// Draws @p text with @p alignment inside @p box (it may overflow the box). Plain text is
    /// elided to the box's width when @p elide is set.
    void draw(QPainter& painter, const QString& text, const QFont& font, const QColor& color,
              const QRectF& box, Qt::Alignment alignment, bool elide = false);

private:
    QTextDocument& document(const QString& text, const QFont& font);

    QHash<QString, std::shared_ptr<QTextDocument>> m_documents;
};

}  // namespace rocketplot
