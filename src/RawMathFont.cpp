#include "RawMathFont.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QChar>
#include <QCoreApplication>
#include <QFont>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRawFont>
#include <QRectF>
#include <QString>
#include <Qt>
#include <QtTypes>
#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

#include "BundledMathFont.h"
#include "Logging.h"
#include "core/GlyphSubstitutions.h"
#include "core/MathFont.h"
#include "core/MathTable.h"
#include "core/OpenTypeReader.h"

namespace rocketplot
{

namespace
{

// The fonts loaded so far, shared by every plot. Emptied when the application ends: a QRawFont
// must not outlive the QGuiApplication.
struct Registry
{
    std::mutex                                            mutex;
    std::shared_ptr<const RawMathFont>                    bundled;
    std::map<QString, std::shared_ptr<const RawMathFont>> installed;  // by family
    bool                                                  emptiedAtEnd = false;
};

Registry& registry()
{
    static Registry s_registry;
    return s_registry;
}

void clearRegistry()
{
    Registry&              fonts = registry();
    const std::scoped_lock lock(fonts.mutex);
    fonts.bundled.reset();
    fonts.installed.clear();
    fonts.emptiedAtEnd = false;
}

// Has the registry emptied when the application ends (the lock held).
void emptyAtEnd(Registry& fonts)
{
    if (!fonts.emptiedAtEnd)
    {
        qAddPostRoutine(clearRegistry);
        fonts.emptiedAtEnd = true;
    }
}

// A font table's bytes, as the readers of tables take them.
std::span<const std::byte> bytesOf(const QByteArray& table)
{
    return std::as_bytes(std::span(table.constData(), static_cast<std::size_t>(table.size())));
}

// The installed math font @p family; null if there is none, or it isn't a math font.
std::shared_ptr<const RawMathFont> loadInstalled(const QString& family)
{
    QFont font(family);
    font.setHintingPreference(QFont::PreferNoHinting);
    const QRawFont raw = QRawFont::fromFont(font);
    if (!raw.isValid() || raw.familyName().compare(family, Qt::CaseInsensitive) != 0)
    {
        return nullptr;  // not installed: Qt chose another font
    }
    const QByteArray math  = raw.fontTable("MATH");
    auto             table = core::MathTable::parse(bytesOf(math));
    if (!table)
    {
        return nullptr;
    }
    const QByteArray substitutions = raw.fontTable("GSUB");
    return std::make_shared<const RawMathFont>(
        raw, std::move(*table), core::GlyphSubstitutions::parse(bytesOf(substitutions)));
}

// The area @p polygon encloses, positive if it goes clockwise in Qt's coordinates (y down).
double signedArea(const QPolygonF& polygon)
{
    double area = 0.0;
    for (qsizetype i = 0; i < polygon.size(); ++i)
    {
        const QPointF& a = polygon.at(i);
        const QPointF& b = polygon.at((i + 1) % polygon.size());
        area += (a.x() * b.y()) - (b.x() * a.y());
    }
    return area / 2.0;
}

}  // namespace

std::shared_ptr<const RawMathFont> RawMathFont::bundled()
{
    Registry&              fonts = registry();
    const std::scoped_lock lock(fonts.mutex);
    if (!fonts.bundled)
    {
        const std::span<const std::byte> bytes = bundledMathFont();
        // The array is static: the font can use it in place.
        const QByteArray data = QByteArray::fromRawData(
            QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())).data(),
            static_cast<qsizetype>(bytes.size()));
        const QRawFont font(data, 1000.0, QFont::PreferNoHinting);
        if (!font.isValid())
        {
            qCWarning(lcRender) << "The bundled math font could not be loaded";
        }
        // Its tables from its bytes, not from Qt's font engine, which may not give them.
        fonts.bundled = std::make_shared<const RawMathFont>(
            font,
            core::MathTable::parse(core::findTable(bytes, "MATH")).value_or(core::MathTable()),
            core::GlyphSubstitutions::parse(core::findTable(bytes, "GSUB")));
        emptyAtEnd(fonts);
    }
    return fonts.bundled;
}

std::shared_ptr<const RawMathFont> RawMathFont::load(const QString& family)
{
    if (family.isEmpty())
    {
        return bundled();
    }
    {
        Registry&              fonts = registry();
        const std::scoped_lock lock(fonts.mutex);
        if (const auto found = fonts.installed.find(family); found != fonts.installed.end())
        {
            return found->second;
        }
    }
    std::shared_ptr<const RawMathFont> font = loadInstalled(family);
    if (!font)
    {
        qCWarning(lcRender).noquote()
            << "No OpenType math font" << family
            << "(one with a MATH table): math is set in Latin Modern Math";
        font = bundled();
    }
    Registry&              fonts = registry();
    const std::scoped_lock lock(fonts.mutex);
    emptyAtEnd(fonts);
    return fonts.installed.emplace(family, std::move(font)).first->second;
}

RawMathFont::RawMathFont(const QRawFont& font, core::MathTable table,
                         core::GlyphSubstitutions substitutions)
  : core::MathFont(std::move(table), std::move(substitutions),
                   font.isValid() ? font.unitsPerEm() : 1000.0),
    m_font(font)
{
    if (!m_font.isValid())
    {
        return;
    }
    if (m_font.pixelSize() != unitsPerEm())
    {
        m_font.setPixelSize(unitsPerEm());
    }
    m_xHeight = -outline(glyph(U'x')).boundingRect().top();
    // The minus sign is a rectangle: its one contour goes the way outer contours go.
    const QList<QPolygonF> minus = outline(glyph(0x2212)).toSubpathPolygons();
    m_clockwise                  = minus.isEmpty() || signedArea(minus.front()) > 0.0;
}

core::GlyphId RawMathFont::glyph(char32_t character) const
{
    const QString          text = QString::fromUcs4(&character, 1);
    std::array<quint32, 2> glyphs{};  // a character outside the BMP is two QChars
    int                    count = static_cast<int>(glyphs.size());
    if (!m_font.glyphIndexesForChars(text.constData(), static_cast<int>(text.size()), glyphs.data(),
                                     &count) ||
        count < 1)
    {
        return 0;
    }
    return glyphs[0];
}

core::GlyphMetrics RawMathFont::metrics(core::GlyphId glyph) const
{
    if (const auto found = m_metrics.constFind(glyph); found != m_metrics.constEnd())
    {
        return *found;
    }
    const QList<QPointF> advances =
        m_font.advancesForGlyphIndexes(QList<quint32>{glyph}, QRawFont::UseDesignMetrics);
    const QRectF ink = outline(glyph).boundingRect();
    return *m_metrics.insert(glyph, {
                                        .advance = advances.isEmpty() ? 0.0 : advances.first().x(),
                                        .xMin    = ink.left(),
                                        .yMin    = -ink.bottom(),
                                        .xMax    = ink.right(),
                                        .yMax    = -ink.top(),
                                    });
}

const QPainterPath& RawMathFont::outline(core::GlyphId glyph) const
{
    if (const auto found = m_outlines.constFind(glyph); found != m_outlines.constEnd())
    {
        return *found;
    }
    return *m_outlines.insert(glyph, m_font.pathForGlyph(glyph));
}

}  // namespace rocketplot
