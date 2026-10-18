#include "rocketplot/text.h"

#include <QString>
#include <QTextDocument>

namespace rocketplot
{

QString literalText(const QString& text)
{
    // Plain text without $ signs is drawn as it is (and elided where there is no room).
    if (!text.contains(u'$') && !Qt::mightBeRichText(text))
    {
        return text;
    }
    // Rich text in which every character is a character: markup escaped, a $ and a backslash as
    // character references (so no $ is left to start math), spaces kept as they are.
    QString escaped = text.toHtmlEscaped();
    escaped.replace(u'$', QStringLiteral("&#36;"));
    escaped.replace(u'\\', QStringLiteral("&#92;"));
    escaped.replace(u'\n', QStringLiteral("<br>"));
    return QStringLiteral(R"(<span style="white-space:pre-wrap">)") + escaped +
           QStringLiteral("</span>");
}

}  // namespace rocketplot
