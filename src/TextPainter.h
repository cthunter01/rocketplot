#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <memory>

#include "MathRenderer.h"

class QPainter;
class QTextDocument;

namespace rocketplot
{

class MathTextObject;

/// @p text without its markup, if it is Qt rich text, and with its math as plain text (for a
/// file's title, a CSV header).
[[nodiscard]] QString plainText(const QString& text);

/// Measures and draws a plot's titles, axis labels and legend names. Plain strings are drawn
/// directly; Qt rich text (Qt::mightBeRichText(): "v<sub>z</sub>", "<i>T</i><sub>0</sub>") and
/// text with math between $ signs ("$v_z$ (m/s)") go through a QTextDocument, cached per text and
/// font, in which each formula is an object that MathTextObject draws.
class TextPainter
{
public:
    TextPainter();
    ~TextPainter();
    TextPainter(const TextPainter&)            = delete;
    TextPainter& operator=(const TextPainter&) = delete;
    TextPainter(TextPainter&&)                 = delete;
    TextPainter& operator=(TextPainter&&)      = delete;

    /// The font of math: an installed OpenType math font's family, or empty for the bundled one.
    [[nodiscard]] QString mathFontFamily() const { return m_math.fontFamily(); }
    void                  setMathFontFamily(const QString& family);

    [[nodiscard]] QSizeF size(const QString& text, const QFont& font);
    /// Draws @p text with @p alignment inside @p box (it may overflow the box). Plain text is
    /// elided to the box's width when @p elide is set.
    void draw(QPainter& painter, const QString& text, const QFont& font, const QColor& color,
              const QRectF& box, Qt::Alignment alignment, bool elide = false);

private:
    /// The document of @p text, or nullptr if it is plain text, drawn as it is.
    QTextDocument* document(const QString& text, const QFont& font);

    // Declared before the documents, which use them until they go.
    MathRenderer                                   m_math;
    std::unique_ptr<MathTextObject>                m_mathObjects;
    QHash<QString, std::shared_ptr<QTextDocument>> m_documents;
};

}  // namespace rocketplot
