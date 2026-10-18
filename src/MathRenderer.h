#pragma once

#include <QHash>
#include <QPainterPath>
#include <QString>
#include <expected>
#include <memory>

class QBrush;
class QPainter;
class QPointF;

namespace rocketplot
{

class RawMathFont;

/// A typeset formula: its outline and extent in font units, y down, with the origin on the
/// baseline at its left end.
struct MathRendering
{
    QPainterPath path;
    double       width      = 0.0;
    double       ascent     = 0.0;
    double       depth      = 0.0;
    double       unitsPerEm = 1000.0;
};

/// Typesets the TeX between a label's $ signs, and draws it as outlines (which look the same on
/// the screen, in an image and in SVG and PDF files, where glyphs would name a font the reader may
/// not have).
class MathRenderer
{
public:
    using Result = std::expected<std::shared_ptr<const MathRendering>, QString>;

    MathRenderer();
    ~MathRenderer();
    MathRenderer(const MathRenderer&)            = delete;
    MathRenderer& operator=(const MathRenderer&) = delete;
    MathRenderer(MathRenderer&&)                 = delete;
    MathRenderer& operator=(MathRenderer&&)      = delete;

    /// The family of the math font: an installed OpenType math font, or empty for the bundled
    /// one (RawMathFont::load()).
    [[nodiscard]] QString fontFamily() const { return m_family; }
    void                  setFontFamily(const QString& family);

    /// @p tex typeset, or why it can't be (logged the first time). Cached.
    [[nodiscard]] Result render(const QString& tex);
    /// Whether there is a font to set math in: none in a library built without its own
    /// (ROCKETPLOT_BUNDLE_MATH_FONT=OFF), unless fontFamily() is an installed math font. Without
    /// one, render() fails, and labels show their formulas as text.
    [[nodiscard]] bool hasFont() const { return m_font != nullptr; }
    /// The font math is set in (if hasFont()).
    [[nodiscard]] const RawMathFont& font() const { return *m_font; }

    /// Draws @p rendering, @p scale pixels to a font unit, with the left end of its baseline at
    /// @p origin.
    static void draw(QPainter& painter, const MathRendering& rendering, QPointF origin,
                     double scale, const QBrush& brush);

private:
    QString                            m_family;
    std::shared_ptr<const RawMathFont> m_font;
    QHash<QString, Result>             m_renderings;
};

}  // namespace rocketplot
