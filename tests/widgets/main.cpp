// Test main for the widget tests: a QApplication on the offscreen platform, so the tests run
// without a display (CI) and render the same everywhere. QT_QPA_PLATFORM, when set, still wins
// (e.g. =xcb to watch them).

#include <QApplication>
#include <QtGlobal>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    // gtest_discover_tests lists the tests without the test environment: don't touch Qt for that.
    if (GTEST_FLAG_GET(list_tests))
    {
        return RUN_ALL_TESTS();
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    const QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
