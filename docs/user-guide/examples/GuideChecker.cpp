#include "GuideChecker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QLatin1String>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSet>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <limits>
#include <utility>

namespace rocketplot::guide
{

namespace
{

QStringList linesOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QString text = QString::fromUtf8(file.readAll());
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (text.endsWith(QLatin1Char('\n')))
    {
        text.chop(1);
    }
    return text.split(QLatin1Char('\n'));
}

QStringList filesIn(const QString& directory, const QString& pattern)
{
    QStringList paths;
    for (const QString& name : QDir(directory).entryList({pattern}, QDir::Files, QDir::Name))
    {
        paths.append(QDir(directory).filePath(name));
    }
    return paths;
}

// @p lines without the indentation they all share, and without empty lines at either end.
QString dedented(QStringList lines)
{
    while (!lines.isEmpty() && lines.constFirst().trimmed().isEmpty())
    {
        lines.removeFirst();
    }
    while (!lines.isEmpty() && lines.constLast().trimmed().isEmpty())
    {
        lines.removeLast();
    }
    qsizetype indent = std::numeric_limits<qsizetype>::max();
    for (const QString& line : std::as_const(lines))
    {
        if (!line.trimmed().isEmpty())
        {
            qsizetype spaces = 0;
            while (spaces < line.size() && line.at(spaces) == QLatin1Char(' '))
            {
                ++spaces;
            }
            indent = std::min(indent, spaces);
        }
    }
    for (QString& line : lines)
    {
        line = line.mid(std::min(indent, line.size()));
        while (line.endsWith(QLatin1Char(' ')))
        {
            line.chop(1);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

// What a chapter is checked for as its lines go by.
class Chapter
{
public:
    Chapter(QString name, const QHash<QString, QString>& snippets,
            const QHash<QString, QString>& outputs)
      : m_name(std::move(name)), m_snippets(&snippets), m_outputs(&outputs)
    {
    }

    // Takes the next line of the chapter; gives the lines that take its place.
    void take(const QString& line, qsizetype number)
    {
        static const QRegularExpression kTag(QStringLiteral("^<!-- (example|output): (\\S+) -->$"));
        static const QRegularExpression kFigure(
            QStringLiteral("!\\[[^\\]]*\\]\\(images/([^)]+)\\.png\\)"));
        const bool fence = line.startsWith(QLatin1String("```"));
        if (m_inBlock)
        {
            if (fence)
            {
                endBlock(number);
                m_lines.append(line);
            }
            else
            {
                m_block.append(line);
            }
            return;
        }
        m_lines.append(line);
        if (fence)
        {
            m_inBlock     = true;
            m_isCpp       = line.trimmed() == QLatin1String("```cpp");
            m_blockTagged = !m_tagKind.isEmpty() || m_sketch;
            return;
        }
        if (const QRegularExpressionMatch tag = kTag.match(line); tag.hasMatch())
        {
            m_tagKind = tag.captured(1);
            m_tagName = tag.captured(2);
        }
        else if (line == QLatin1String("<!-- sketch -->"))
        {
            m_sketch = true;
        }
        else if (!line.trimmed().isEmpty())
        {
            clearTag();  // a tag is for the block right after it
            for (auto match = kFigure.globalMatch(line); match.hasNext();)
            {
                figures.insert(match.next().captured(1));
            }
        }
    }

    [[nodiscard]] QString text() const
    {
        return m_lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    }

    QStringList   problems;
    QSet<QString> used;  // "example: name" and "output: name" tags seen
    QSet<QString> figures;

private:
    void endBlock(qsizetype number)
    {
        m_inBlock = false;
        if (m_tagKind.isEmpty())
        {
            if (m_isCpp && !m_blockTagged)
            {
                problems.append(QStringLiteral("%1:%2: a C++ block that is neither an example "
                                               "nor marked <!-- sketch -->")
                                    .arg(m_name)
                                    .arg(number));
            }
            m_lines.append(m_block);
        }
        else
        {
            const auto&   source = m_tagKind == QLatin1String("example") ? *m_snippets : *m_outputs;
            const QString key    = m_tagKind + QLatin1String(": ") + m_tagName;
            used.insert(key);
            if (!source.contains(m_tagName))
            {
                problems.append(
                    QStringLiteral("%1:%2: there is no %3").arg(m_name).arg(number).arg(key));
                m_lines.append(m_block);
            }
            else
            {
                const QString expected = source.value(m_tagName);
                if (m_block.join(QLatin1Char('\n')) != expected)
                {
                    problems.append(QStringLiteral("%1:%2: the block differs from %3")
                                        .arg(m_name)
                                        .arg(number)
                                        .arg(key));
                }
                m_lines.append(expected.split(QLatin1Char('\n')));
            }
        }
        m_block.clear();
        clearTag();
    }

    void clearTag()
    {
        m_tagKind.clear();
        m_tagName.clear();
        m_sketch = false;
    }

    QString                        m_name;
    const QHash<QString, QString>* m_snippets;
    const QHash<QString, QString>* m_outputs;
    QStringList                    m_lines;  // the chapter, brought up to date
    QStringList                    m_block;  // the lines of the block being read
    QString                        m_tagKind;
    QString                        m_tagName;
    bool                           m_sketch      = false;
    bool                           m_inBlock     = false;
    bool                           m_isCpp       = false;
    bool                           m_blockTagged = false;
};

}  // namespace

GuideChecker::GuideChecker(QString guideDirectory) : m_directory(std::move(guideDirectory)) { }

QHash<QString, QString> GuideChecker::snippets() const
{
    static const QRegularExpression kMarker(QStringLiteral("^// \\[(/?)([^\\]]+)\\]$"));
    QHash<QString, QStringList>     found;
    const QString sources = QDir(m_directory).filePath(QStringLiteral("examples"));
    for (const QString& path : filesIn(sources, QStringLiteral("*.cpp")))
    {
        QStringList open;
        for (const QString& line : linesOf(path))
        {
            const QRegularExpressionMatch marker = kMarker.match(line.trimmed());
            if (!marker.hasMatch())
            {
                for (const QString& name : std::as_const(open))
                {
                    found[name].append(line);
                }
            }
            else if (marker.captured(1).isEmpty())
            {
                open.append(marker.captured(2));
            }
            else
            {
                open.removeAll(marker.captured(2));
            }
        }
    }
    QHash<QString, QString> result;
    for (auto snippet = found.cbegin(); snippet != found.cend(); ++snippet)
    {
        result.insert(snippet.key(), dedented(snippet.value()));
    }
    return result;
}

QHash<QString, QString> GuideChecker::refreshed(QStringList* problems) const
{
    const QHash<QString, QString> examples = snippets();
    QHash<QString, QString>       chapters;
    QSet<QString>                 used;
    QSet<QString>                 figures;
    for (const QString& path : filesIn(m_directory, QStringLiteral("*.md")))
    {
        Chapter           chapter(QFileInfo(path).fileName(), examples, m_outputs);
        const QStringList lines = linesOf(path);
        for (qsizetype i = 0; i < lines.size(); ++i)
        {
            chapter.take(lines.at(i), i + 1);
        }
        chapters.insert(path, chapter.text());
        used.unite(chapter.used);
        figures.unite(chapter.figures);
        problems->append(chapter.problems);
    }
    // Nothing the examples have is left out of the guide, and the guide shows no figure they
    // don't draw.
    for (auto example = examples.cbegin(); example != examples.cend(); ++example)
    {
        if (!used.contains(QLatin1String("example: ") + example.key()))
        {
            problems->append(QStringLiteral("example %1 is not in the guide").arg(example.key()));
        }
    }
    for (auto output = m_outputs.cbegin(); output != m_outputs.cend(); ++output)
    {
        if (!used.contains(QLatin1String("output: ") + output.key()))
        {
            problems->append(QStringLiteral("output %1 is not in the guide").arg(output.key()));
        }
    }
    for (const QString& figure : m_figures)
    {
        if (!figures.contains(figure))
        {
            problems->append(QStringLiteral("figure %1 is not in the guide").arg(figure));
        }
    }
    for (const QString& figure : std::as_const(figures))
    {
        const QString image = QStringLiteral("images/%1.png").arg(figure);
        if (!m_figures.contains(figure))
        {
            problems->append(
                QStringLiteral("the guide shows %1, which no example draws").arg(image));
        }
        else if (!QFileInfo::exists(QDir(m_directory).filePath(image)))
        {
            problems->append(QStringLiteral("%1 has not been drawn").arg(image));
        }
    }
    problems->sort();
    return chapters;
}

QStringList GuideChecker::check() const
{
    QStringList problems;
    (void)refreshed(&problems);
    return problems;
}

QStringList GuideChecker::update() const
{
    QStringList                   problems;
    const QHash<QString, QString> chapters = refreshed(&problems);
    QStringList                   changed;
    for (auto chapter = chapters.cbegin(); chapter != chapters.cend(); ++chapter)
    {
        QFile file(chapter.key());
        if (!file.open(QIODevice::ReadOnly))
        {
            continue;
        }
        const QByteArray text = chapter.value().toUtf8();
        const QByteArray old  = file.readAll().replace("\r\n", "\n");
        file.close();
        if (text != old && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(text);
            changed.append(QFileInfo(chapter.key()).fileName());
        }
    }
    changed.sort();
    return changed;
}

}  // namespace rocketplot::guide
