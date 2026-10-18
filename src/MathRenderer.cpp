#include "MathRenderer.h"

#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringLiteral>
#include <QTransform>
#include <Qt>
#include <QtTypes>
#include <algorithm>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>

#include "Logging.h"
#include "RawMathFont.h"
#include "core/MathBox.h"
#include "core/MathLayout.h"
#include "core/MathParser.h"

namespace rocketplot
{

namespace
{

// Renderings kept; the cache starts over beyond this (labels rarely change).
constexpr qsizetype kMaxRenderings = 256;

// A rule as a contour that goes the way the font's outer contours go, so that where it overlaps
// a glyph (the bar of a radical, its surd) the two add up instead of cancelling out.
void addRule(QPainterPath& path, const core::MathRule& rule, bool clockwise)
{
    const double top    = -(rule.y + rule.height);
    const double bottom = -rule.y;
    const double left   = rule.x;
    const double right  = rule.x + rule.width;
    // Clockwise with y down: top left, top right, bottom right, bottom left.
    QPolygonF corners(
        {QPointF(left, top), QPointF(right, top), QPointF(right, bottom), QPointF(left, bottom)});
    if (!clockwise)
    {
        std::ranges::reverse(corners);
    }
    path.addPolygon(corners);
    path.closeSubpath();
}

MathRenderer::Result typeset(const QString& tex, const RawMathFont& font)
{
    const auto list = core::parseMath(tex.toStdString());
    if (!list)
    {
        return std::unexpected(QStringLiteral("%1 (at %2)")
                                   .arg(QString::fromStdString(list.error().message))
                                   .arg(list.error().position));
    }
    const core::MathBox box       = core::layoutMath(*list, font);
    auto                rendering = std::make_shared<MathRendering>();
    rendering->path.setFillRule(Qt::WindingFill);
    for (const core::PositionedGlyph& glyph : box.glyphs)
    {
        QTransform placement;
        placement.translate(glyph.x, -glyph.y);
        placement.scale(glyph.scale, glyph.scale);
        rendering->path.addPath(placement.map(font.outline(glyph.glyph)));
    }
    for (const core::MathRule& rule : box.rules)
    {
        addRule(rendering->path, rule, font.clockwise());
    }
    rendering->width      = box.width;
    rendering->ascent     = box.ascent;
    rendering->depth      = box.depth;
    rendering->unitsPerEm = font.unitsPerEm();
    return rendering;
}

}  // namespace

MathRenderer::MathRenderer() : m_font(RawMathFont::bundled()) { }

MathRenderer::~MathRenderer() = default;

void MathRenderer::setFontFamily(const QString& family)
{
    if (family == m_family)
    {
        return;
    }
    m_family = family;
    m_font   = RawMathFont::load(family);
    m_renderings.clear();
}

MathRenderer::Result MathRenderer::render(const QString& tex)
{
    if (const auto found = m_renderings.constFind(tex); found != m_renderings.constEnd())
    {
        return *found;
    }
    if (!m_font)
    {
        static std::once_flag s_warned;
        std::call_once(s_warned, [] {
            qCWarning(lcRender) << "rocketplot was built without its math font "
                                   "(ROCKETPLOT_BUNDLE_MATH_FONT=OFF): math is shown as text";
        });
        return std::unexpected(QStringLiteral("no math font"));
    }
    if (m_renderings.size() >= kMaxRenderings)
    {
        m_renderings.clear();
    }
    Result result = typeset(tex, *m_font);
    if (!result)
    {
        qCWarning(lcRender).noquote() << "Can't typeset" << tex << ':' << result.error();
    }
    return *m_renderings.insert(tex, std::move(result));
}

void MathRenderer::draw(QPainter& painter, const MathRendering& rendering, QPointF origin,
                        double scale, const QBrush& brush)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(origin);
    painter.scale(scale, scale);
    painter.fillPath(rendering.path, brush);
    painter.restore();
}

}  // namespace rocketplot
