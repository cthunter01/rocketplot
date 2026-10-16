#pragma once

#include <QHash>
#include <QPainterPath>
#include <QRawFont>
#include <QString>
#include <QtTypes>
#include <memory>

#include "core/GlyphSubstitutions.h"
#include "core/MathFont.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot
{

/// An OpenType math font at one pixel per font unit, through Qt's QRawFont: the glyphs math is
/// typeset with, their metrics and outlines. Loaded once and shared by every plot; never added to
/// the application's fonts (QFontDatabase), which are the application's business.
class RawMathFont final : public core::MathFont
{
public:
    /// Latin Modern Math, which the library has inside it.
    [[nodiscard]] static std::shared_ptr<const RawMathFont> bundled();
    /// The installed font @p family ("STIX Two Math"), if it is an OpenType math font (one with
    /// a MATH table); otherwise, with a warning, the bundled one. Empty: the bundled one.
    [[nodiscard]] static std::shared_ptr<const RawMathFont> load(const QString& family);

    RawMathFont(const QRawFont& font, core::MathTable table,
                core::GlyphSubstitutions substitutions);

    [[nodiscard]] core::GlyphId      glyph(char32_t character) const override;
    [[nodiscard]] core::GlyphMetrics metrics(core::GlyphId glyph) const override;

    /// The outline of @p glyph in font units, y down as Qt has it, its origin on the baseline
    /// (cached).
    [[nodiscard]] const QPainterPath& outline(core::GlyphId glyph) const;
    /// The height of a lower case x, in font units.
    [[nodiscard]] double xHeight() const { return m_xHeight; }
    /// Whether the outer contours of the glyphs go clockwise (y down): a rule drawn with them must
    /// too, or where it overlaps a glyph, the two would cancel out.
    [[nodiscard]] bool clockwise() const { return m_clockwise; }

private:
    QRawFont                                   m_font;
    double                                     m_xHeight   = 0.0;
    bool                                       m_clockwise = true;
    mutable QHash<quint32, QPainterPath>       m_outlines;
    mutable QHash<quint32, core::GlyphMetrics> m_metrics;
};

}  // namespace rocketplot
