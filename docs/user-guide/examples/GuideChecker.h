#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace rocketplot::guide
{

/// Keeps the guide's Markdown and its examples the same.
///
/// In the guide, a line `<!-- example: name -->` goes before a fenced block of C++ that is the
/// code between `// [name]` and `// [/name]` in one of the example sources, and a line
/// `<!-- output: name -->` before a block of text that an example produces. A C++ block that is
/// neither (a fragment that wouldn't compile by itself) has `<!-- sketch -->` before it. Figures
/// are the images the examples draw: `![...](images/name.png)`.
class GuideChecker
{
public:
    /// @p guideDirectory holds the chapters (*.md); the example sources are in its examples/.
    explicit GuideChecker(QString guideDirectory);

    /// What the examples produce, by name: set before checking or updating.
    void setOutputs(const QHash<QString, QString>& outputs) { m_outputs = outputs; }
    void setFigures(const QStringList& names) { m_figures = names; }

    /// What is wrong, one line each: a block that differs from its example, an example the guide
    /// doesn't show, a C++ block without a tag, a figure without an image or the other way round.
    /// Empty when all is well.
    [[nodiscard]] QStringList check() const;

    /// Rewrites the blocks of the guide from the examples and the outputs. Returns the chapters
    /// it changed.
    [[nodiscard]] QStringList update() const;

    /// The code between the markers of every example in the sources, by name, without their
    /// common indentation.
    [[nodiscard]] QHash<QString, QString> snippets() const;

private:
    // The chapters with their blocks brought up to date; with @p problems, what was wrong.
    [[nodiscard]] QHash<QString, QString> refreshed(QStringList* problems) const;

    QString                 m_directory;
    QHash<QString, QString> m_outputs;
    QStringList             m_figures;
};

}  // namespace rocketplot::guide
