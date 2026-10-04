// Main of the widget benchmarks: a QApplication on the offscreen platform, like the widget tests,
// so that they run anywhere and measure the library's rendering rather than a display's.
// QT_QPA_PLATFORM, when set, still wins.

#include <QApplication>
#include <QtGlobal>

#include <benchmark/benchmark.h>

int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    const QApplication app(argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
