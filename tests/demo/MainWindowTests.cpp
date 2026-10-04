#include "MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QIODevice>
#include <QMimeData>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QString>
#include <QTemporaryDir>
#include <Qt>

#include <gtest/gtest.h>

#include "DataImportWidget.h"
#include "PropertyInspector.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::demo::DataImportWidget;
using rocketplot::demo::MainWindow;

class MainWindowTest : public testing::Test
{
protected:
    void SetUp() override { m_window.resize(1200, 800); }

    // The index of the page called @p title, or -1.
    [[nodiscard]] int pageCalled(const QString& title) const
    {
        for (int page = 0; page < m_window.pageCount(); ++page)
        {
            if (m_window.pageTitle(page) == title)
            {
                return page;
            }
        }
        return -1;
    }

    // Shows a page, and expects it to have a plot, to draw, and to show its source.
    void showAndCheck(int page)
    {
        m_window.selectPage(page);
        const auto* source = m_window.findChild<QPlainTextEdit*>();
        ASSERT_NE(source, nullptr);
        EXPECT_FALSE(m_window.plots().isEmpty());
        EXPECT_FALSE(m_window.grab().isNull());
        EXPECT_TRUE(source->toPlainText().contains("plot"));
        EXPECT_FALSE(source->toPlainText().contains("demo.qrc"));  // "... is missing from"
    }

    MainWindow m_window;
};

TEST_F(MainWindowTest, EveryPageShowsAPlotAndItsSource)
{
    ASSERT_GT(m_window.pageCount(), 15);
    for (int page = 0; page < m_window.pageCount(); ++page)
    {
        const testing::ScopedTrace trace(__FILE__, __LINE__,
                                         m_window.pageTitle(page).toStdString());
        showAndCheck(page);
    }
}

TEST_F(MainWindowTest, TheInspectorListsThePlotsOfThePage)
{
    const int linked = pageCalled("Linked plots");
    ASSERT_GE(linked, 0);

    m_window.selectPage(linked);
    EXPECT_EQ(m_window.plots().size(), 3);
    EXPECT_EQ(m_window.inspector()->topLevelItemCount(), 3);

    m_window.selectPage(0);
    EXPECT_EQ(m_window.inspector()->topLevelItemCount(), 1);
    EXPECT_NE(m_window.inspector()->propertyItem(m_window.plots().constFirst(), "title"), nullptr);
}

TEST_F(MainWindowTest, TheThemeChoiceAppliesToEveryPage)
{
    m_window.selectTheme(rocketplot::ThemeMode::DARK);
    EXPECT_EQ(m_window.plots().constFirst()->themeMode(), rocketplot::ThemeMode::DARK);

    m_window.selectPage(1);
    EXPECT_EQ(m_window.plots().constFirst()->themeMode(), rocketplot::ThemeMode::DARK);
}

TEST_F(MainWindowTest, OpensAFileOnTheDataPage)
{
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath("flight.csv");
    QFile         file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("t,v\n0,1\n1,2\n");
    file.close();

    ASSERT_TRUE(m_window.openData(path));

    EXPECT_EQ(m_window.currentPage(), pageCalled("Your data"));
    const auto* importer = qobject_cast<const DataImportWidget*>(m_window.page());
    ASSERT_NE(importer, nullptr);
    EXPECT_EQ(importer->table().rows, 2U);
    EXPECT_EQ(m_window.plots().constFirst()->title(), QString("flight.csv"));
    EXPECT_FALSE(m_window.openData(directory.filePath("missing.csv")));
}

TEST_F(MainWindowTest, ATableDroppedAnywhereIsPlottedOnTheDataPage)
{
    QMimeData data;
    data.setText("x;y\n1;2\n2;4\n3;6\n");
    // A drop goes to whoever took the drag when it came in.
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QDropEvent      drop(QPoint(20, 20), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);

    QApplication::sendEvent(&m_window, &enter);
    ASSERT_TRUE(enter.isAccepted());
    QApplication::sendEvent(&m_window, &drop);

    EXPECT_EQ(m_window.currentPage(), pageCalled("Your data"));
    const auto* importer = qobject_cast<const DataImportWidget*>(m_window.page());
    ASSERT_NE(importer, nullptr);
    EXPECT_EQ(importer->table().rows, 3U);
    EXPECT_EQ(importer->table().separator, ';');
}

}  // namespace
