#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QString>
#include <QStringList>
#include <QTimer>
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
// Time each page gets before its screenshot (the live page needs a moment to collect data).
constexpr int kSettleMilliseconds = 1500;

void wait(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

// Shows every page in turn and saves the window as <directory>/<NN>-<page>.png.
int saveScreenshots(rocketplot::demo::MainWindow& window, const QString& directory)
{
    if (!QDir().mkpath(directory))
    {
        std::println(stderr, "cannot create {}", directory.toStdString());
        return 1;
    }
    for (int page = 0; page < window.pageCount(); ++page)
    {
        window.selectPage(page);
        wait(kSettleMilliseconds);
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
        parser.addOption(theme);
        parser.addOption(screenshots);
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
        window.resize(kWidth, kHeight);
        window.show();
        if (parser.isSet(screenshots))
        {
            return saveScreenshots(window, parser.value(screenshots));
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
