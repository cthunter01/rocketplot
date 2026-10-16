#include "MathTextObject.h"

#include <QAbstractTextDocumentLayout>
#include <QBrush>
#include <QChar>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <optional>

#include "MathRenderer.h"
#include "RawMathFont.h"

namespace rocketplot
{

namespace
{

// Where the format of a formula's character keeps the formula and its size.
constexpr int kTexProperty   = QTextFormat::UserProperty + 1;
constexpr int kScaleProperty = QTextFormat::UserProperty + 2;  // pixels per font unit
// The object laid out to see how Qt places objects (MathTextObject::roundsDescents()).
constexpr int    kProbeProperty = QTextFormat::UserProperty + 3;
constexpr double kProbeHeight   = 1000.0;

// Math against text of a large x-height (most sans-serif fonts) is enlarged up to this much, so
// that its letters don't look small among the text's.
constexpr double kMaxSizeFactor = 1.25;

// The em of @p font in pixels, as text in it is laid out.
double emPixels(const QFont& font)
{
    if (font.pixelSize() > 0)
    {
        return font.pixelSize();
    }
    return font.pointSizeF() * QFontMetricsF(font).fontDpi() / 72.0;
}

// The em of math among text in @p font, in pixels: the text's em, enlarged when the text's
// x-height is larger than the math font's.
double mathEmPixels(const QFont& font, const RawMathFont& mathFont)
{
    const double em = emPixels(font);
    if (em <= 0.0 || mathFont.xHeight() <= 0.0)
    {
        return em;
    }
    const double textXHeight = QFontMetricsF(font).xHeight() / em;
    const double mathXHeight = mathFont.xHeight() / mathFont.unitsPerEm();
    return em * std::clamp(textXHeight / mathXHeight, 1.0, kMaxSizeFactor);
}

// The descent of @p font, rounded as QFontMetrics has it or not.
double descentOf(const QFont& font, bool rounded)
{
    return rounded ? QFontMetrics(font).descent() : QFontMetricsF(font).descent();
}

// @p font, enlarged until its descent is at least @p depth pixels. A formula's character has it:
// Qt puts an object of AlignBaseline that far below the baseline.
QFont metricFont(QFont font, double depth, bool rounded)
{
    int    pixels  = std::max(1, static_cast<int>(std::round(emPixels(font))));
    double descent = descentOf(font, rounded);
    while (descent < depth)
    {
        // In proportion, and at least a pixel more: rounding makes descents grow unevenly.
        const double grown = std::ceil(pixels * depth / std::max(1.0, descent));
        pixels             = std::max(pixels + 1, static_cast<int>(grown));
        font.setPixelSize(pixels);
        descent = descentOf(font, rounded);
    }
    return font;
}

}  // namespace

MathTextObject::MathTextObject(MathRenderer& renderer) : m_renderer(&renderer) { }

bool MathTextObject::roundsDescents()
{
    static const bool kRounded = probeDescents();
    return kRounded;
}

bool MathTextObject::probeDescents()
{
    // A size at which the two descents differ, if the font has one.
    QTextDocument document;
    QFont         font = document.defaultFont();
    for (int pixels = 37; pixels < 64; ++pixels)
    {
        font.setPixelSize(pixels);
        if (std::abs(QFontMetricsF(font).descent() - QFontMetrics(font).descent()) > 0.1)
        {
            break;
        }
    }
    document.setDocumentMargin(0.0);
    document.documentLayout()->registerHandler(kObjectType, this);
    QTextCharFormat probe;
    probe.setObjectType(kObjectType);
    probe.setVerticalAlignment(QTextCharFormat::AlignBaseline);
    probe.setFont(font);
    probe.setProperty(kProbeProperty, true);
    QTextCursor(&document).insertText(QString(QChar::ObjectReplacementCharacter), probe);
    (void)document.size();  // lays it out

    const double descent = kProbeHeight - document.begin().layout()->lineAt(0).ascent();
    return std::abs(descent - QFontMetrics(font).descent()) <
           std::abs(descent - QFontMetricsF(font).descent());
}

std::optional<QTextCharFormat> MathTextObject::format(const QString&         tex,
                                                      const QTextCharFormat& around,
                                                      const QFont&           documentFont)
{
    const auto rendering = m_renderer->render(tex);
    if (!rendering)
    {
        return std::nullopt;
    }
    const QFont  font  = around.font().resolve(documentFont);
    const double scale = mathEmPixels(font, m_renderer->font()) / (*rendering)->unitsPerEm;

    QTextCharFormat format = around;
    format.setObjectType(kObjectType);
    format.setVerticalAlignment(QTextCharFormat::AlignBaseline);
    format.setFont(metricFont(font, (*rendering)->depth * scale, roundsDescents()));
    format.setProperty(kTexProperty, tex);
    format.setProperty(kScaleProperty, scale);
    return format;
}

QSizeF MathTextObject::intrinsicSize(QTextDocument* /*document*/, int /*position*/,
                                     const QTextFormat& format)
{
    if (format.hasProperty(kProbeProperty))
    {
        return {1.0, kProbeHeight};
    }
    const auto rendering = m_renderer->render(format.stringProperty(kTexProperty));
    if (!rendering)
    {
        return {};
    }
    // Qt makes the height less the descent of the format's font the object's ascent: with that
    // descent added, the ascent is the formula's own.
    const double descent = descentOf(format.toCharFormat().font(), roundsDescents());
    const double scale   = format.doubleProperty(kScaleProperty);
    return {(*rendering)->width * scale, ((*rendering)->ascent * scale) + descent};
}

void MathTextObject::drawObject(QPainter* painter, const QRectF& rect, QTextDocument* /*document*/,
                                int /*position*/, const QTextFormat& format)
{
    const auto rendering = m_renderer->render(format.stringProperty(kTexProperty));
    if (!rendering)
    {
        return;
    }
    // The pen is the text's colour here: the format's, or the document's.
    const double scale = format.doubleProperty(kScaleProperty);
    MathRenderer::draw(*painter, **rendering,
                       QPointF(rect.left(), rect.top() + ((*rendering)->ascent * scale)), scale,
                       painter->pen().brush());
}

}  // namespace rocketplot
