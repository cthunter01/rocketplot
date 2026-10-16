#pragma once

#include <QObject>
#include <QSizeF>
#include <QTextFormat>
#include <QTextObjectInterface>
#include <optional>

class QFont;
class QPainter;
class QRectF;
class QString;
class QTextDocument;

namespace rocketplot
{

class MathRenderer;

/// The formulas of a label in its QTextDocument: each is an object character whose format holds
/// its TeX, which this handler measures and draws. A formula sits on the baseline of the text
/// around it, at the size of that text, in its colour (so HTML's font-size and color apply).
// NOLINTNEXTLINE(misc-multiple-inheritance): a handler is a QObject and the interface
class MathTextObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    static constexpr int kObjectType = QTextFormat::UserObject + 1;

    explicit MathTextObject(MathRenderer& renderer);

    /// The format of the character that stands for the formula @p tex among text of format
    /// @p around, in a document whose default font is @p documentFont; nullopt if @p tex can't be
    /// typeset.
    [[nodiscard]] std::optional<QTextCharFormat> format(const QString&         tex,
                                                        const QTextCharFormat& around,
                                                        const QFont&           documentFont);

    QSizeF intrinsicSize(QTextDocument* document, int position, const QTextFormat& format) override;
    void   drawObject(QPainter* painter, const QRectF& rect, QTextDocument* document, int position,
                      const QTextFormat& format) override;

private:
    /// Whether Qt gives an object of AlignBaseline the descent of its format's font as
    /// QFontMetrics rounds it (Qt 6.8) or as QFontMetricsF has it (later versions): found out
    /// once, by laying out an object.
    [[nodiscard]] bool roundsDescents();
    [[nodiscard]] bool probeDescents();

    MathRenderer* m_renderer;
};

}  // namespace rocketplot
