// rocketplot_guide_examples: the examples of the user guide (docs/user-guide), as a program.
//
//   rocketplot_guide_examples                   runs every example
//   rocketplot_guide_examples --check           and checks that the guide shows them as they are
//   rocketplot_guide_examples --update          or rewrites the guide's blocks from them
//   rocketplot_guide_examples --figures <dir>   draws the guide's figures into <dir>
//
// It runs on Qt's offscreen platform unless QT_QPA_PLATFORM says otherwise, so it needs no display.

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEvent>
#include <QHash>
#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QWidget>
#include <Qt>
#include <QtGlobal>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <print>

#include "Examples.h"
#include "GuideChecker.h"
#include "rocketplot/PlotWidget.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::guide::Examples;
using rocketplot::guide::Figure;
using rocketplot::guide::GuideChecker;

// The widget of @p figure as it looks, with the pointer where the figure wants it.
QImage draw(const Figure& figure)
{
    const std::unique_ptr<QWidget> widget(figure.make(nullptr));
    widget->resize(figure.size);
    if (figure.pointer)
    {
        (void)widget->grab();  // lays everything out: the pointer needs a plot area to be over
        auto* plot = qobject_cast<PlotWidget*>(widget.get());
        if (plot == nullptr)
        {
            plot = widget->findChild<PlotWidget*>();
        }
        if (plot != nullptr)
        {
            const QPointF at = figure.pointer(*plot);
            QMouseEvent   move(QEvent::MouseMove, at, plot->mapToGlobal(at), Qt::NoButton,
                               Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(plot, &move);
        }
    }
    return widget->grab().toImage();
}

// Draws @p figure; saves it into @p directory if one is given. False if it can't be.
bool drawFigure(const Figure& figure, const QString& directory)
{
    const QImage image = draw(figure);
    if (image.isNull())
    {
        std::println(stderr, "figure {} is empty", figure.name.toStdString());
        return false;
    }
    if (directory.isEmpty())
    {
        return true;
    }
    const QString path = QDir(directory).filePath(figure.name + QStringLiteral(".png"));
    if (!image.save(path))
    {
        std::println(stderr, "cannot write {}", path.toStdString());
        return false;
    }
    std::println("{}", path.toStdString());
    return true;
}

// Draws every figure, until one fails.
bool drawFigures(const Examples& examples, const QString& directory)
{
    if (!directory.isEmpty() && !QDir().mkpath(directory))
    {
        std::println(stderr, "cannot create {}", directory.toStdString());
        return false;
    }
    return std::ranges::all_of(examples.figures, [&directory](const Figure& figure) {
        return drawFigure(figure, directory);
    });
}

int run(const QCommandLineParser& parser, const QCommandLineOption& figures,
        const QCommandLineOption& check, const QCommandLineOption& update,
        const QCommandLineOption& guide)
{
    const Examples      examples = rocketplot::guide::allExamples();
    const QTemporaryDir scratch;
    for (const auto& demonstration : examples.demonstrations)
    {
        demonstration.run(scratch.path());
    }
    if (!drawFigures(examples, parser.value(figures)))
    {
        return EXIT_FAILURE;
    }

    GuideChecker            checker(parser.value(guide));
    QHash<QString, QString> outputs;
    for (const auto& output : examples.outputs)
    {
        outputs.insert(output.name, output.text());
    }
    QStringList names;
    for (const Figure& figure : examples.figures)
    {
        names.append(figure.name);
    }
    checker.setOutputs(outputs);
    checker.setFigures(names);
    if (parser.isSet(update))
    {
        for (const QString& chapter : checker.update())
        {
            std::println("updated {}", chapter.toStdString());
        }
    }
    if (parser.isSet(check))
    {
        const QStringList problems = checker.check();
        for (const QString& problem : problems)
        {
            std::println(stderr, "{}", problem.toStdString());
        }
        if (!problems.isEmpty())
        {
            std::println(stderr,
                         "The guide and its examples differ. After changing an example, "
                         "run this program with --update (and --figures).");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* argv[])
{
    try
    {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        const QApplication app(argc, argv);
        QCommandLineParser parser;
        parser.setApplicationDescription(
            QStringLiteral("Runs the examples of rocketplot's user guide."));
        parser.addHelpOption();
        const QCommandLineOption figures(
            QStringList{QStringLiteral("figures")},
            QStringLiteral("Draw the guide's figures into <directory>."),
            QStringLiteral("directory"));
        const QCommandLineOption check(
            QStringList{QStringLiteral("check")},
            QStringLiteral("Fail if the guide's blocks differ from the examples."));
        const QCommandLineOption update(
            QStringList{QStringLiteral("update")},
            QStringLiteral("Rewrite the guide's blocks from the examples."));
        const QCommandLineOption guide(QStringList{QStringLiteral("guide")},
                                       QStringLiteral("The guide's folder (default: the one this "
                                                      "program was built from)."),
                                       QStringLiteral("directory"),
                                       QStringLiteral(ROCKETPLOT_GUIDE_DIR));
        parser.addOptions({figures, check, update, guide});
        parser.process(app);
        return run(parser, figures, check, update, guide);
    }
    catch (const std::exception& e)
    {
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
        return EXIT_FAILURE;
    }
}
