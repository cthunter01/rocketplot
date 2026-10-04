#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <Qt>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>

#include "MainWindow.h"
#include "rocketplot/enums.h"

namespace
{

constexpr int kWidth  = 1400;
constexpr int kHeight = 900;
// Time each page gets before its screenshot, unless --settle says otherwise (the live pages need
// a moment to collect data).
constexpr int kSettleMilliseconds = 1500;

void wait(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

// The page whose title is @p title (however capitalized), or -1.
int pageCalled(const rocketplot::demo::MainWindow& window, const QString& title)
{
    for (int page = 0; page < window.pageCount(); ++page)
    {
        if (window.pageTitle(page).compare(title, Qt::CaseInsensitive) == 0)
        {
            return page;
        }
    }
    return -1;
}

// Shows every page in turn (or only page @p only, if it isn't -1) and saves the window as
// <directory>/<NN>-<page>.png.
int saveScreenshots(rocketplot::demo::MainWindow& window, const QString& directory, int settle,
                    int only)
{
    if (!QDir().mkpath(directory))
    {
        std::println(stderr, "cannot create {}", directory.toStdString());
        return 1;
    }
    for (int page = 0; page < window.pageCount(); ++page)
    {
        if (only >= 0 && page != only)
        {
            continue;
        }
        window.selectPage(page);
        wait(settle);
        const QString name =
            QStringLiteral("%1-%2.png")
                .arg(page + 1, 2, 10, QLatin1Char('0'))
                .arg(window.pageTitle(page).toLower().replace(QLatin1Char(' '), QLatin1Char('-')));
        const QString path = QDir(directory).filePath(name);
        if (!window.grab().save(path))
        {
            std::println(stderr, "cannot write {}", path.toStdString());
            return 1;
        }
        std::println("{}", path.toStdString());
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    try
    {
        const QApplication app(argc, argv);
        QApplication::setApplicationName(QStringLiteral("rocketplot demo"));

        QCommandLineParser parser;
        parser.setApplicationDescription(QStringLiteral("A gallery of rocketplot's features."));
        parser.addHelpOption();
        const QCommandLineOption theme(
            QStringList{QStringLiteral("theme")},
            QStringLiteral(
                "Start with this plot theme: system, light, dark, high-contrast or print."),
            QStringLiteral("theme"), QStringLiteral("system"));
        const QCommandLineOption screenshots(
            QStringList{QStringLiteral("screenshots")},
            QStringLiteral("Save a screenshot of every page into <directory>, then quit "
                           "(works without a display: QT_QPA_PLATFORM=offscreen)."),
            QStringLiteral("directory"));
        const QCommandLineOption settle(
            QStringList{QStringLiteral("settle")},
            QStringLiteral("With --screenshots: wait this long before each one (default: %1).")
                .arg(kSettleMilliseconds),
            QStringLiteral("milliseconds"), QString::number(kSettleMilliseconds));
        const QCommandLineOption inspector(
            QStringList{QStringLiteral("inspector")},
            QStringLiteral("Start with the property inspector shown."));
        const QCommandLineOption page(
            QStringList{QStringLiteral("page")},
            QStringLiteral("Start on this page, e.g. \"Telemetry\" (with --screenshots: only it)."),
            QStringLiteral("title"));
        parser.addOption(theme);
        parser.addOption(page);
        parser.addOption(screenshots);
        parser.addOption(settle);
        parser.addOption(inspector);
        parser.addPositionalArgument(
            QStringLiteral("file"),
            QStringLiteral("A table of numbers (CSV, TSV, ...) to plot on the \"Your data\" page."),
            QStringLiteral("[file]"));
        parser.process(app);

        rocketplot::demo::MainWindow window;
        const QString                themeName = parser.value(theme).toLower();
        if (themeName == QLatin1String("light"))
        {
            window.selectTheme(rocketplot::ThemeMode::LIGHT);
        }
        else if (themeName == QLatin1String("dark"))
        {
            window.selectTheme(rocketplot::ThemeMode::DARK);
        }
        else if (themeName == QLatin1String("high-contrast"))
        {
            window.selectTheme(rocketplot::ThemeMode::HIGH_CONTRAST);
        }
        else if (themeName == QLatin1String("print"))
        {
            window.selectTheme(rocketplot::ThemeMode::PRINT);
        }
        const int first = parser.isSet(page) ? pageCalled(window, parser.value(page)) : -1;
        if (parser.isSet(page) && first < 0)
        {
            std::println(stderr, "no page called \"{}\"", parser.value(page).toStdString());
            return EXIT_FAILURE;
        }
        window.setInspectorVisible(parser.isSet(inspector));
        window.resize(kWidth, kHeight);
        window.show();
        if (first >= 0)
        {
            window.selectPage(first);
        }
        if (!parser.positionalArguments().isEmpty())
        {
            window.openData(parser.positionalArguments().constFirst());
        }
        if (parser.isSet(screenshots))
        {
            return saveScreenshots(window, parser.value(screenshots),
                                   std::max(0, parser.value(settle).toInt()), first);
        }
        return QApplication::exec();
    }
    catch (const std::exception& e)
    {
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
        return EXIT_FAILURE;
    }
}
