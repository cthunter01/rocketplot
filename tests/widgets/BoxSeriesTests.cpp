#include "rocketplot/BoxSeries.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QTest>
#include <Qt>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/BoxStatistics.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::BoxPart;
using rocketplot::BoxSeries;
using rocketplot::BoxStatistics;
using rocketplot::PlotWidget;
using rocketplot::Range;

// A box from 3 to 7 with whiskers at 1 and 9, an outlier at 0.2 and a far-out at 9.8.
BoxStatistics sample()
{
    return {.lowerWhisker = 1.0,
            .q1           = 3.0,
            .median       = 5.0,
            .q3           = 7.0,
            .upperWhisker = 9.0,
            .mean         = 5.5,
            .outliers     = {0.2},
            .farOuts      = {9.8}};
}

TEST(BoxSeries, ItsPointsAreItsStatistics)
{
    PlotWidget       plot;
    const BoxSeries* box = plot.addBox(sample(), 2.0, "Nominal");
    EXPECT_EQ(box->kind(), rocketplot::SeriesKind::BOX);
    ASSERT_EQ(box->size(), 8U);
    EXPECT_EQ(box->x(0), 2.0);
    EXPECT_EQ(box->y(2), 5.0);
    EXPECT_EQ(box->partAt(2), BoxPart::MEDIAN);
    EXPECT_EQ(box->partAt(5), BoxPart::MEAN);
    EXPECT_EQ(box->partAt(6), BoxPart::OUTLIER);
    EXPECT_EQ(box->partAt(7), BoxPart::FAR_OUT);
    // The legend reads the median over the box, and nothing beside it.
    EXPECT_EQ(box->nearestIndex(2.1), 2U);
    EXPECT_FALSE(box->nearestIndex(3.0));
    EXPECT_EQ(box->valueAt(1.9), 5.0);
}

TEST(BoxSeries, TheStatisticsMustBeFiniteAndInOrder)
{
    PlotWidget    plot;
    BoxStatistics wrong = sample();
    wrong.q1            = 6.0;  // above the median
    EXPECT_THROW(plot.addBox(wrong, 0.0), std::invalid_argument);
    wrong.q1     = 3.0;
    wrong.median = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(plot.addBox(wrong, 0.0), std::invalid_argument);
    EXPECT_TRUE(plot.series().isEmpty());
    // Outliers and a mean that aren't numbers are left out.
    BoxStatistics loose = sample();
    loose.outliers.push_back(std::numeric_limits<double>::infinity());
    loose.mean     = std::numeric_limits<double>::quiet_NaN();
    BoxSeries* box = plot.addBox(loose, 0.0);
    EXPECT_EQ(box->statistics().outliers.size(), 1U);
    EXPECT_FALSE(box->statistics().mean);
    EXPECT_THROW(box->setData(std::vector{1.0}, std::vector{1.0}), std::logic_error);
    EXPECT_THROW(box->setYErrors(std::vector{1.0}), std::logic_error);
}

TEST(BoxSeries, AutoscaleCoversTheFarOutsAndTheWidth)
{
    PlotWidget plot;
    BoxSeries* box = plot.addBox(sample(), 2.0);
    EXPECT_LE(plot.yAxis()->min(), 0.2);
    EXPECT_GE(plot.yAxis()->max(), 9.8);
    EXPECT_LE(plot.xAxis()->min(), 1.75);  // half of 0.5 either side
    EXPECT_GE(plot.xAxis()->max(), 2.25);
    box->setWidth(2.0);
    EXPECT_LE(plot.xAxis()->min(), 1.0);
    // Lying down, the values are along x.
    box->setOrientation(Qt::Horizontal);
    EXPECT_EQ(box->x(2), 5.0);
    EXPECT_EQ(box->y(2), 2.0);
    EXPECT_LE(plot.xAxis()->min(), 0.2);
    EXPECT_GE(plot.xAxis()->max(), 9.8);
    EXPECT_LE(plot.yAxis()->min(), 1.0);
}

TEST(BoxSeries, StatisticsFromValues)
{
    const BoxStatistics box =
        BoxStatistics::fromValues(std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 17, 100});
    EXPECT_DOUBLE_EQ(box.q1, 3.5);
    EXPECT_DOUBLE_EQ(box.q3, 8.5);
    EXPECT_EQ(box.outliers, (std::vector<double>{17.0}));
    EXPECT_EQ(box.farOuts, (std::vector<double>{100.0}));
}

TEST(BoxSeries, ItsSettingsAreSaved)
{
    PlotWidget plot;
    BoxSeries* box = plot.addBox(sample(), 0.0, "Nominal");
    box->setOrientation(Qt::Horizontal);
    box->setWidth(0.3);
    box->setFillOpacity(0.6);
    box->setLineWidth(1.5);
    box->setMedianColor(Qt::red);
    box->setMeanVisible(true);
    box->setWhiskerCapRatio(0.8);
    box->setFarOutMarker(rocketplot::Marker::PLUS);
    PlotWidget other;
    BoxSeries* restored = other.addBox(sample(), 0.0, "Nominal");
    ASSERT_TRUE(other.restoreState(plot.saveState()));
    EXPECT_EQ(restored->orientation(), Qt::Horizontal);
    EXPECT_EQ(restored->width(), 0.3);
    EXPECT_EQ(restored->fillOpacity(), 0.6);
    EXPECT_EQ(restored->lineWidth(), 1.5);
    EXPECT_EQ(restored->medianColor(), QColor(Qt::red));
    EXPECT_TRUE(restored->isMeanVisible());
    EXPECT_EQ(restored->whiskerCapRatio(), 0.8);
    EXPECT_EQ(restored->farOutMarker(), rocketplot::Marker::PLUS);
}

TEST(BoxSeries, TheCsvNamesTheParts)
{
    PlotWidget plot;
    plot.addBox(sample(), 0.0, "Apogee");
    const QString csv = plot.toCsv();
    EXPECT_TRUE(csv.startsWith("x,Apogee,Apogee (part)\n0,1,lower whisker\n")) << csv.toStdString();
    EXPECT_TRUE(csv.contains("0,9.8,far-out\n"));
}

class BoxRenderingTest : public rocketplot::test::RenderedPlotTest
{
protected:
    // The first and last rows from @p from to @p to in which @p column is @p color.
    static std::pair<int, int> rowsOf(const QImage& image, const QColor& color, int column,
                                      int from, int to)
    {
        int first = -1;
        int last  = -1;
        for (int y = from; y <= to; ++y)
        {
            if (image.pixelColor(column, y) == color)
            {
                first = first < 0 ? y : first;
                last  = y;
            }
        }
        return {first, last};
    }

    BoxSeries* addSample()
    {
        BoxSeries* box = m_plot.addBox(sample(), 5.0, "Nominal");
        box->setWidth(2.0);
        box->setLineWidth(1.0);
        box->setMedianColor(Qt::red);
        m_plot.xAxis()->setRange(0.0, 10.0);
        m_plot.yAxis()->setRange(0.0, 10.0);
        return box;
    }
};

TEST_F(BoxRenderingTest, TheBoxIsWhereItsQuartilesAre)
{
    const QImage without = render();
    BoxSeries*   box     = addSample();
    const QImage with    = render();
    // The box's ink between its whiskers' ends: from the upper cap to the lower one.
    const QRect ink = inkRect(without, with, QRect(pixelAt(3.0, 8.5), pixelAt(7.0, 1.5)));
    EXPECT_NEAR(ink.left(), m_plot.mapFromData(QPointF(4.0, 0.0)).x(), 1.5);
    EXPECT_NEAR(ink.right(), m_plot.mapFromData(QPointF(6.0, 0.0)).x(), 1.5);
    // The top and bottom of the box: rows across it that are all outline.
    const QColor outline     = box->color();
    const int    q3          = qRound(m_plot.mapFromData(QPointF(0.0, 7.0)).y());
    const int    q1          = qRound(m_plot.mapFromData(QPointF(0.0, 3.0)).y());
    const int    middle      = qRound(m_plot.mapFromData(QPointF(5.3, 0.0)).x());
    const auto [top, bottom] = rowsOf(with, outline, middle, q3 - 3, q1 + 3);
    EXPECT_NEAR(top, q3, 1);
    EXPECT_NEAR(bottom, q1, 1);
    // The median across it, in its own color: a line one pixel wide covers the row it is in.
    const int median = static_cast<int>(std::floor(m_plot.mapFromData(QPointF(0.0, 5.0)).y()));
    EXPECT_EQ(with.pixelColor(middle, median), QColor(Qt::red));
}

TEST_F(BoxRenderingTest, OutliersAndFarOutsHaveTheirOwnShapes)
{
    const QImage without = render();
    addSample();
    const QImage with  = render();
    const auto   shape = [&](double value) {
        const QPoint center = pixelAt(5.0, value);
        return inkRect(without, with, QRect(center - QPoint(8, 8), center + QPoint(8, 8)));
    };
    const QRect outlier = shape(0.2);
    const QRect farOut  = shape(9.8);
    ASSERT_FALSE(outlier.isEmpty());
    ASSERT_FALSE(farOut.isEmpty());
    // A hollow circle has nothing in its middle; a cross does.
    const QPoint outlierMiddle = pixelAt(5.0, 0.2);
    const QPoint farOutMiddle  = pixelAt(5.0, 9.8);
    EXPECT_EQ(with.pixel(outlierMiddle), without.pixel(outlierMiddle));
    EXPECT_NE(with.pixel(farOutMiddle), without.pixel(farOutMiddle));
}

TEST_F(BoxRenderingTest, AnExportLooksLikeTheWidget)
{
    addSample()->setMeanVisible(true);
    m_plot.legend()->setVisible(true);
    EXPECT_EQ(m_plot.renderToImage().convertToFormat(QImage::Format_RGB32),
              render().convertToFormat(QImage::Format_RGB32));
}

TEST(BoxCrosshair, ItSnapsToTheMedian)
{
    PlotWidget plot;
    plot.resize(600, 400);
    plot.addBox(sample(), 0.0);
    plot.setCrosshairEnabled(true);
    plot.setCrosshairMode(rocketplot::CrosshairMode::SNAP);
    plot.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&plot));
    QTest::mouseMove(&plot, QPoint(1, 1));
    QTest::mouseMove(&plot, plot.mapFromData(QPointF(0.0, 5.1)).toPoint());
    const auto* box = qobject_cast<const BoxSeries*>(plot.crosshairSeries());
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->partAt(plot.crosshairIndex().value_or(0)), BoxPart::MEDIAN);
}

TEST(CategoryAxis, ItShowsItsCategories)
{
    PlotWidget plot;
    plot.xAxis()->setScaleType(rocketplot::ScaleType::CATEGORY);
    plot.xAxis()->setCategories({"Nominal", "Gusty", "Engine-out"});
    EXPECT_EQ(plot.xAxis()->range(), (Range{.min = -0.5, .max = 2.5}));
    plot.addBox(sample(), 1.0);
    EXPECT_EQ(plot.xAxis()->range(), (Range{.min = -0.5, .max = 2.5}));  // no margin
    // Data beyond them widens it.
    plot.addBox(sample(), 4.0);
    EXPECT_EQ(plot.xAxis()->max(), 4.25);
    // Not saved: like the label, they come from the code.
    PlotWidget restored;
    ASSERT_TRUE(restored.restoreState(plot.saveState()));
    EXPECT_EQ(restored.xAxis()->scaleType(), rocketplot::ScaleType::CATEGORY);
    EXPECT_TRUE(restored.xAxis()->categories().isEmpty());
}

}  // namespace
