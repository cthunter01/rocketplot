// The widgets in a form made with Qt Designer: PlotForm.ui holds what Designer writes for a
// PlotWidget and a PlotGrid with every one of their properties set, and uic turns it into the
// code that an application compiles.

#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "rocketplot/PlotGrid.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/enums.h"
#include "ui_PlotForm.h"

namespace
{

class FormTest : public testing::Test
{
protected:
    void SetUp() override { m_ui.setupUi(&m_form); }

    QWidget      m_form;
    Ui::PlotForm m_ui{};
};

TEST_F(FormTest, AFormSetsAPlotsProperties)
{
    const rocketplot::PlotWidget* plot = m_ui.plot;

    ASSERT_NE(plot, nullptr);
    EXPECT_EQ(plot->title(), QString("Ascent"));
    EXPECT_EQ(plot->themeMode(), rocketplot::ThemeMode::DARK);
    EXPECT_TRUE(plot->isCrosshairEnabled());
    EXPECT_EQ(plot->crosshairMode(), rocketplot::CrosshairMode::TRACE);
    EXPECT_FALSE(plot->debugOverlay());
}

TEST_F(FormTest, AFormSetsAGridsProperties)
{
    const rocketplot::PlotGrid* grid = m_ui.plotGrid;

    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(grid->rowCount(), 3);
    EXPECT_EQ(grid->columnCount(), 2);
    EXPECT_EQ(grid->plots().size(), 6);
    EXPECT_EQ(grid->xLink(), rocketplot::GridLink::ALL);
    EXPECT_TRUE(grid->areInnerTickLabelsVisible());
    EXPECT_EQ(grid->spacing(), 4);
    EXPECT_EQ(grid->title(), QString("Flight 7"));
    EXPECT_EQ(grid->themeMode(), rocketplot::ThemeMode::PRINT);
    EXPECT_TRUE(grid->isCrosshairEnabled());
    // Whatever order the form set them in, the plots made along the way have them all.
    EXPECT_EQ(grid->plot(2, 1)->themeMode(), rocketplot::ThemeMode::PRINT);
    EXPECT_TRUE(grid->plot(2, 1)->isCrosshairEnabled());
    EXPECT_EQ(grid->crosshairMode(), rocketplot::CrosshairMode::SNAP);
    EXPECT_EQ(grid->plot(2, 1)->crosshairMode(), rocketplot::CrosshairMode::SNAP);
}

TEST_F(FormTest, TheFormDraws)
{
    m_form.resize(800, 600);

    EXPECT_FALSE(m_form.grab().isNull());
}

}  // namespace
