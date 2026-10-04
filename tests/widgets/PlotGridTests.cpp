#include "rocketplot/PlotGrid.h"

#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTemporaryDir>
#include <array>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::GridLink;
using rocketplot::PlotGrid;
using rocketplot::PlotWidget;
using rocketplot::Range;
using rocketplot::Theme;
using rocketplot::ThemeMode;

constexpr std::array<double, 3> kX{0.0, 1.0, 2.0};
constexpr std::array<double, 3> kSmall{0.0, 1.0, 2.0};
constexpr std::array<double, 3> kLarge{-1234567.0, 0.0, 7654321.0};

// How many pixels of @p area in @p image aren't @p background.
int inked(const QImage& image, QRect area, const QColor& background)
{
    area      = area.intersected(image.rect());
    int count = 0;
    for (int y = area.top(); y <= area.bottom(); ++y)
    {
        for (int x = area.left(); x <= area.right(); ++x)
        {
            count += image.pixelColor(x, y) != background ? 1 : 0;
        }
    }
    return count;
}

TEST(PlotGrid, StartsWithOnePlot)
{
    const PlotGrid grid;

    EXPECT_EQ(grid.rowCount(), 1);
    EXPECT_EQ(grid.columnCount(), 1);
    ASSERT_EQ(grid.plots().size(), 1);
    EXPECT_EQ(grid.plot(0, 0), grid.plots().constFirst());
    EXPECT_EQ(grid.plot(0), grid.plots().constFirst());
}

TEST(PlotGrid, HasAPlotInEveryCell)
{
    const PlotGrid grid(3, 2);

    EXPECT_EQ(grid.rowCount(), 3);
    EXPECT_EQ(grid.columnCount(), 2);
    ASSERT_EQ(grid.plots().size(), 6);
    EXPECT_EQ(grid.plot(0, 0), grid.plots().at(0));
    EXPECT_EQ(grid.plot(0, 1), grid.plots().at(1));
    EXPECT_EQ(grid.plot(2, 1), grid.plots().at(5));
    EXPECT_EQ(grid.plot(3, 0), nullptr);
    EXPECT_EQ(grid.plot(0, 2), nullptr);
    EXPECT_EQ(grid.plot(-1, 0), nullptr);
    EXPECT_EQ(grid.plot(1, 1)->parentWidget(), &grid);
}

TEST(PlotGrid, HasAtLeastOnePlotAndNotThousands)
{
    PlotGrid grid(0, -3);
    EXPECT_EQ(grid.plots().size(), 1);

    grid.setGridSize(1000, 1);
    EXPECT_LE(grid.rowCount(), 64);
    EXPECT_EQ(grid.plots().size(), grid.rowCount());
}

TEST(PlotGrid, ThePlotsShareItsAreaEvenly)
{
    PlotGrid grid(3, 2);
    grid.resize(600, 600);
    ASSERT_FALSE(grid.grab().isNull());  // a widget that isn't shown places its children when drawn

    EXPECT_EQ(grid.plot(0, 0)->geometry(), QRect(0, 0, 300, 200));
    EXPECT_EQ(grid.plot(0, 1)->geometry(), QRect(300, 0, 300, 200));
    EXPECT_EQ(grid.plot(2, 1)->geometry(), QRect(300, 400, 300, 200));
}

TEST(PlotGrid, SpacingSetsThePlotsApart)
{
    PlotGrid grid(2, 2);
    grid.resize(610, 410);

    grid.setSpacing(10);

    EXPECT_EQ(grid.spacing(), 10);
    EXPECT_EQ(grid.plot(0, 0)->geometry(), QRect(0, 0, 300, 200));
    EXPECT_EQ(grid.plot(1, 1)->geometry(), QRect(310, 210, 300, 200));
}

TEST(PlotGrid, RowsAndColumnsCanTakeMoreOfTheRoom)
{
    PlotGrid grid(2, 2);
    grid.resize(600, 400);

    grid.setRowStretch(0, 3);
    grid.setColumnStretch(1, 2);

    EXPECT_EQ(grid.rowStretch(0), 3);
    EXPECT_EQ(grid.rowStretch(1), 1);
    EXPECT_EQ(grid.rowStretch(5), 0);
    EXPECT_EQ(grid.plot(0, 0)->geometry(), QRect(0, 0, 200, 300));
    EXPECT_EQ(grid.plot(1, 1)->geometry(), QRect(200, 300, 400, 100));
}

TEST(PlotGrid, GrowingKeepsThePlotsThereAre)
{
    PlotGrid          grid(2, 2);
    const PlotWidget* corner = grid.plot(0, 0);
    const PlotWidget* inner  = grid.plot(1, 1);

    grid.setGridSize(3, 3);

    EXPECT_EQ(grid.plots().size(), 9);
    EXPECT_EQ(grid.plot(0, 0), corner);
    EXPECT_EQ(grid.plot(1, 1), inner);
    EXPECT_NE(grid.plot(2, 2), nullptr);
}

TEST(PlotGrid, ShrinkingDeletesThePlotsOutside)
{
    PlotGrid                   grid(2, 2);
    const QPointer<PlotWidget> kept(grid.plot(0, 0));
    const QPointer<PlotWidget> gone(grid.plot(1, 1));

    grid.setRowCount(1);
    grid.setColumnCount(1);

    EXPECT_EQ(grid.plots().size(), 1);
    EXPECT_EQ(grid.plot(0, 0), kept.data());
    EXPECT_TRUE(gone.isNull());
}

TEST(PlotGrid, ThePlotsOfAColumnShareTheirXAxis)
{
    const PlotGrid grid(2, 2);
    EXPECT_EQ(grid.xLink(), GridLink::COLUMNS);

    grid.plot(0, 0)->xAxis()->setRange(2.0, 3.0);

    EXPECT_EQ(grid.plot(1, 0)->xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    EXPECT_NE(grid.plot(0, 1)->xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    ASSERT_NE(grid.link(0), nullptr);
    EXPECT_NE(grid.link(0), grid.link(1));
    EXPECT_EQ(grid.link(0)->plots().size(), 2);
    EXPECT_EQ(grid.plot(1, 0)->link(), grid.link(0));
    EXPECT_EQ(grid.link(2), nullptr);
}

TEST(PlotGrid, AllThePlotsCanShareTheirXAxis)
{
    PlotGrid grid(2, 2);

    grid.setXLink(GridLink::ALL);
    grid.plot(1, 1)->xAxis()->setRange(2.0, 3.0);

    for (const PlotWidget* plot : grid.plots())
    {
        EXPECT_EQ(plot->xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    }
    EXPECT_EQ(grid.link(0), grid.link(1));
}

TEST(PlotGrid, OrNoneOfThem)
{
    PlotGrid grid(2, 1);

    grid.setXLink(GridLink::NONE);
    grid.plot(0)->xAxis()->setRange(2.0, 3.0);

    EXPECT_NE(grid.plot(1)->xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
    EXPECT_EQ(grid.link(0), nullptr);
    EXPECT_EQ(grid.plot(0)->link(), nullptr);
}

TEST(PlotGrid, APlotAddedLaterJoinsItsColumn)
{
    PlotGrid grid(1, 1);
    grid.plot(0)->xAxis()->setRange(2.0, 3.0);

    grid.setRowCount(2);

    EXPECT_EQ(grid.plot(1)->xAxis()->range(), (Range{.min = 2.0, .max = 3.0}));
}

TEST(PlotGrid, OnlyTheBottomRowLabelsASharedXAxis)
{
    PlotGrid grid(3, 2);

    EXPECT_FALSE(grid.areInnerTickLabelsVisible());
    EXPECT_FALSE(grid.plot(0, 0)->xAxis()->areTickLabelsVisible());
    EXPECT_FALSE(grid.plot(1, 1)->xAxis()->areTickLabelsVisible());
    EXPECT_TRUE(grid.plot(2, 0)->xAxis()->areTickLabelsVisible());
    EXPECT_TRUE(grid.plot(2, 1)->xAxis()->areTickLabelsVisible());
    EXPECT_TRUE(grid.plot(0, 0)->yAxis()->areTickLabelsVisible());

    grid.setRowCount(4);  // the row that was at the bottom no longer is
    EXPECT_FALSE(grid.plot(2, 0)->xAxis()->areTickLabelsVisible());
    EXPECT_TRUE(grid.plot(3, 0)->xAxis()->areTickLabelsVisible());

    grid.setInnerTickLabelsVisible(true);
    EXPECT_TRUE(grid.plot(0, 0)->xAxis()->areTickLabelsVisible());
}

TEST(PlotGrid, PlotsWithTheirOwnXAxisAllLabelIt)
{
    PlotGrid grid(2, 1);

    grid.setXLink(GridLink::NONE);

    EXPECT_TRUE(grid.plot(0)->xAxis()->areTickLabelsVisible());
    EXPECT_TRUE(grid.plot(1)->xAxis()->areTickLabelsVisible());
}

TEST(PlotGrid, PlotAreasLineUpInRowsAndColumns)
{
    PlotGrid grid(2, 2);
    grid.resize(800, 600);
    grid.setInnerTickLabelsVisible(true);
    // Wide labels in one plot, a title and an axis label in another, a right axis in a third.
    grid.plot(0, 0)->addLine(kX, kSmall);
    grid.plot(1, 0)->addLine(kX, kLarge);
    grid.plot(1, 0)->yAxis()->setNumberFormat(rocketplot::NumberFormat::PLAIN);
    grid.plot(0, 1)->setTitle("A title");
    grid.plot(0, 1)->xAxis()->setLabel("Time (s)");
    grid.plot(1, 1)->addLine(kX, kLarge)->setOnSecondaryYAxis(true);
    const QImage shown = grid.grab().toImage();
    ASSERT_FALSE(shown.isNull());

    // Each plot's area is in its own coordinates, and the cells are all the same size.
    const auto area = [&grid](int row, int column) { return grid.plot(row, column)->plotArea(); };
    EXPECT_DOUBLE_EQ(area(0, 0).left(), area(1, 0).left());
    EXPECT_DOUBLE_EQ(area(0, 0).right(), area(1, 0).right());
    EXPECT_DOUBLE_EQ(area(0, 1).left(), area(1, 1).left());
    EXPECT_DOUBLE_EQ(area(0, 1).right(), area(1, 1).right());
    EXPECT_DOUBLE_EQ(area(0, 0).top(), area(0, 1).top());
    EXPECT_DOUBLE_EQ(area(0, 0).bottom(), area(0, 1).bottom());
    EXPECT_DOUBLE_EQ(area(1, 0).top(), area(1, 1).top());
    EXPECT_DOUBLE_EQ(area(1, 0).bottom(), area(1, 1).bottom());
    // And only as far in as its own row and column need: the wide labels are in the left column.
    EXPECT_GT(area(0, 0).left(), area(0, 1).left());
    EXPECT_GT(area(0, 0).top(), area(1, 0).top());  // the title is in the top row
}

TEST(PlotGrid, TheTitleGoesAboveThePlots)
{
    PlotGrid grid(2, 1);
    grid.resize(400, 400);
    grid.setThemeMode(ThemeMode::LIGHT);
    EXPECT_EQ(grid.plot(0)->geometry().top(), 0);
    const QImage plain = grid.grab().toImage();

    grid.setTitle("Flight 7");
    const QImage titled = grid.grab().toImage();

    const int top = grid.plot(0)->geometry().top();
    EXPECT_GT(top, 10);
    EXPECT_EQ(grid.plot(1)->geometry().bottom(), 399);
    EXPECT_EQ(grid.plot(0)->height(), grid.plot(1)->height());
    const QRect above(0, 0, 400, top);
    EXPECT_GT(inked(titled, above, Theme::light().background), 0);
    EXPECT_NE(plain, titled);
}

TEST(PlotGrid, TheThemeIsThatOfEveryPlot)
{
    PlotGrid grid(1, 2);
    grid.resize(400, 200);
    grid.setSpacing(20);

    grid.setThemeMode(ThemeMode::DARK);
    grid.setRowCount(2);

    EXPECT_EQ(grid.themeMode(), ThemeMode::DARK);
    for (const PlotWidget* plot : grid.plots())
    {
        EXPECT_EQ(plot->themeMode(), ThemeMode::DARK);
    }
    EXPECT_EQ(grid.theme(), Theme::dark());
    // The gap between the plots is the grid's to fill.
    EXPECT_EQ(grid.grab().toImage().pixelColor(200, 50), Theme::dark().background);
}

TEST(PlotGrid, ACustomThemeIsThatOfEveryPlotToo)
{
    PlotGrid grid(1, 1);
    Theme    theme   = Theme::light();
    theme.background = QColor(Qt::yellow);

    grid.setTheme(theme);
    grid.setColumnCount(2);

    EXPECT_EQ(grid.themeMode(), ThemeMode::CUSTOM);
    EXPECT_EQ(grid.plot(0, 1)->theme().background, QColor(Qt::yellow));
}

TEST(PlotGrid, TheCrosshairIsThatOfEveryPlot)
{
    PlotGrid grid(2, 1);

    grid.setCrosshairEnabled(true);
    grid.setRowCount(3);

    EXPECT_TRUE(grid.isCrosshairEnabled());
    for (const PlotWidget* plot : grid.plots())
    {
        EXPECT_TRUE(plot->isCrosshairEnabled());
    }
}

TEST(PlotGrid, ResetViewTurnsAutoscaleBackOnEverywhere)
{
    PlotGrid grid(2, 2);
    for (const PlotWidget* plot : grid.plots())
    {
        plot->xAxis()->setRange(2.0, 3.0);
        plot->yAxis()->setRange(2.0, 3.0);
    }

    grid.resetView();

    for (const PlotWidget* plot : grid.plots())
    {
        EXPECT_TRUE(plot->xAxis()->autoscale());
        EXPECT_TRUE(plot->yAxis()->autoscale());
    }
}

TEST(PlotGrid, RendersEveryPlotIntoOneImage)
{
    PlotGrid grid(2, 2);
    grid.resize(300, 300);
    grid.setThemeMode(ThemeMode::DARK);

    const QImage image = grid.renderToImage({.size = {600, 400}, .dpi = 96.0});

    ASSERT_EQ(image.size(), QSize(600, 400));
    const QColor background = Theme::dark().background;
    EXPECT_GT(inked(image, QRect(0, 0, 300, 200), background), 100);
    EXPECT_GT(inked(image, QRect(300, 0, 300, 200), background), 100);
    EXPECT_GT(inked(image, QRect(0, 200, 300, 200), background), 100);
    EXPECT_GT(inked(image, QRect(300, 200, 300, 200), background), 100);
    EXPECT_EQ(image.pixelColor(0, 0), background);
}

TEST(PlotGrid, AnExportCanHaveAThemeOfItsOwn)
{
    PlotGrid grid(2, 1);
    grid.resize(300, 300);
    grid.setThemeMode(ThemeMode::DARK);
    grid.setTitle("Flight 7");

    const QImage image =
        grid.renderToImage({.size = {300, 300}, .dpi = 96.0, .theme = Theme::print()});

    EXPECT_EQ(image.pixelColor(1, 1), Theme::print().background);
    EXPECT_EQ(image.pixelColor(298, 298), Theme::print().background);
    EXPECT_EQ(grid.theme(), Theme::dark());  // only for the export
}

TEST(PlotGrid, AnExportAtAnotherSizeAndSharpness)
{
    PlotGrid grid(1, 2);
    grid.resize(400, 200);

    EXPECT_EQ(grid.renderToImage({.dpi = 96.0}).size(), QSize(400, 200));
    EXPECT_EQ(grid.renderToImage({.size = {400, 200}, .dpi = 192.0}).size(), QSize(800, 400));
    EXPECT_TRUE(grid.renderToImage({.size = {100'000, 100'000}, .dpi = 96.0}).isNull());
}

TEST(PlotGrid, WritesImagesAndDrawings)
{
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    PlotGrid grid(2, 1);
    grid.resize(400, 300);
    grid.setTitle("Flight 7");
    grid.plot(0)->addLine(kX, kSmall);

    for (const char* name : {"grid.png", "grid.svg", "grid.pdf"})
    {
        const QString file = directory.filePath(name);
        EXPECT_TRUE(grid.exportTo(file)) << name;
        EXPECT_GT(QFileInfo(file).size(), 500) << name;
    }
    EXPECT_FALSE(grid.exportTo(directory.filePath("no/such/folder/grid.png")));
    EXPECT_FALSE(grid.exportSvg(directory.filePath("no/such/folder/grid.svg")));
    EXPECT_FALSE(grid.exportPdf(directory.filePath("no/such/folder/grid.pdf")));
}

TEST(PlotGrid, SavesAndRestoresTheStateOfEveryPlot)
{
    PlotGrid saved(2, 1);
    saved.plot(0)->yAxis()->setRange(1.0, 2.0);
    saved.plot(1)->yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    saved.plot(1)->setCrosshairEnabled(true);
    PlotGrid restored(2, 1);

    ASSERT_TRUE(restored.restoreState(saved.saveState()));

    EXPECT_EQ(restored.plot(0)->yAxis()->range(), (Range{.min = 1.0, .max = 2.0}));
    EXPECT_EQ(restored.plot(1)->yAxis()->scaleType(), rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_TRUE(restored.plot(1)->isCrosshairEnabled());
    EXPECT_FALSE(restored.plot(0)->isCrosshairEnabled());
    EXPECT_FALSE(restored.restoreState(QJsonObject()));
}

TEST(PlotGrid, AStateFromAnotherGridIsAppliedAsFarAsItGoes)
{
    PlotGrid saved(1, 1);
    saved.plot(0)->yAxis()->setRange(1.0, 2.0);
    saved.plot(0)->xAxis()->setTickLabelsVisible(true);
    PlotGrid restored(3, 1);

    ASSERT_TRUE(restored.restoreState(saved.saveState()));

    EXPECT_EQ(restored.plot(0)->yAxis()->range(), (Range{.min = 1.0, .max = 2.0}));
    EXPECT_TRUE(restored.plot(1)->yAxis()->autoscale());
    // Which plots label a shared axis stays the grid's business.
    EXPECT_FALSE(restored.plot(0)->xAxis()->areTickLabelsVisible());
}

TEST(PlotGrid, AsksForMoreRoomTheMorePlotsItHas)
{
    const PlotGrid small(1, 1);
    const PlotGrid large(3, 2);

    EXPECT_GT(large.sizeHint().width(), small.sizeHint().width());
    EXPECT_GT(large.sizeHint().height(), small.sizeHint().height());
    EXPECT_GT(large.minimumSizeHint().height(), small.minimumSizeHint().height());
}

TEST(PlotGrid, APlotOutlivesNothingOfAGridThatIsDeleted)
{
    auto*                   grid = new PlotGrid(2, 2);
    const QPointer<QObject> link(grid->link(0));
    const QPointer<QWidget> plot(grid->plot(1, 1));
    grid->resize(300, 300);
    ASSERT_FALSE(grid->grab().isNull());

    delete grid;

    EXPECT_TRUE(link.isNull());
    EXPECT_TRUE(plot.isNull());
}

}  // namespace
