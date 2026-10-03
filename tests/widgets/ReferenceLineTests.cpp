#include "rocketplot/ReferenceLine.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <Qt>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::ReferenceLine;

using ReferenceLineTest = rocketplot::test::RenderedPlotTest;

TEST(ReferenceLine, Defaults)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    const ReferenceLine* horizontal = plot.addHorizontalLine(2.5, "limit");
    const ReferenceLine* vertical   = plot.addVerticalLine(7.0);
    EXPECT_EQ(horizontal->orientation(), Qt::Horizontal);
    EXPECT_EQ(vertical->orientation(), Qt::Vertical);
    EXPECT_DOUBLE_EQ(horizontal->value(), 2.5);
    EXPECT_EQ(horizontal->label(), "limit");
    EXPECT_TRUE(vertical->label().isEmpty());
    EXPECT_EQ(horizontal->lineStyle(), Qt::DashLine);
    EXPECT_DOUBLE_EQ(horizontal->lineWidth(), plot.theme().annotationLineWidth);
    EXPECT_EQ(horizontal->labelAlignment(), Qt::AlignRight | Qt::AlignTop);
}

TEST(ReferenceLine, SettersSignalOnlyChanges)
{
    PlotWidget       plot;
    ReferenceLine*   line = plot.addHorizontalLine(1.0);
    const QSignalSpy changed(line, &rocketplot::Annotation::changed);
    line->setValue(2.0);
    line->setValue(2.0);
    line->setLabel("limit");
    line->setLineWidth(3.0);
    line->setLineWidth(-1.0);  // ignored
    line->setLineStyle(Qt::SolidLine);
    line->setLabelAlignment(Qt::AlignLeft | Qt::AlignBottom);
    EXPECT_EQ(changed.count(), 5);
    EXPECT_DOUBLE_EQ(line->lineWidth(), 3.0);
    line->resetLineWidth();
    EXPECT_DOUBLE_EQ(line->lineWidth(), plot.theme().annotationLineWidth);
}

TEST_F(ReferenceLineTest, RunsAcrossThePlotAtItsValue)
{
    const QImage   plain      = render();
    ReferenceLine* horizontal = m_plot.addHorizontalLine(4.0);
    horizontal->setColor(Qt::red);
    horizontal->setLineStyle(Qt::SolidLine);
    horizontal->setLineWidth(3.0);
    ReferenceLine* vertical = m_plot.addVerticalLine(7.0);
    vertical->setColor(Qt::blue);
    vertical->setLineStyle(Qt::SolidLine);
    vertical->setLineWidth(3.0);
    const QImage image = render();
    for (const double x : {0.1, 3.3, 9.9})
    {
        EXPECT_EQ(image.pixelColor(pixelAt(x, 4.0)), QColor(Qt::red)) << x;
    }
    for (const double y : {0.1, 6.6, 9.9})
    {
        EXPECT_EQ(image.pixelColor(pixelAt(7.0, y)), QColor(Qt::blue)) << y;
    }
    // Nowhere else, and not outside the plot area.
    EXPECT_EQ(image.pixelColor(pixelAt(3.3, 4.2)), plain.pixelColor(pixelAt(3.3, 4.2)));
    EXPECT_EQ(image.pixelColor(pixelAt(-0.3, 4.0)), plain.pixelColor(pixelAt(-0.3, 4.0)));
}

TEST_F(ReferenceLineTest, DashedInTheThemeColorByDefault)
{
    m_plot.addHorizontalLine(4.0);
    const QImage image = render();
    // A line one pixel wide is drawn in the pixel row its value falls in.
    const int    row   = static_cast<int>(std::floor(m_plot.mapFromData(QPointF(0.0, 4.0)).y()));
    int          drawn = 0;
    const QRectF area  = m_plot.plotArea();
    for (int x = static_cast<int>(area.left()) + 1; x < static_cast<int>(area.right()); ++x)
    {
        drawn += image.pixelColor(x, row) == m_plot.theme().annotation ? 1 : 0;
    }
    // Dashes: most of the row, with gaps.
    EXPECT_GT(drawn, static_cast<int>(area.width() * 0.4));
    EXPECT_LT(drawn, static_cast<int>(area.width() * 0.9));
}

TEST_F(ReferenceLineTest, ALineTheAxisCannotPlaceIsNotDrawn)
{
    const QImage   plain = render();
    ReferenceLine* line  = m_plot.addHorizontalLine(20.0, "out of view");
    EXPECT_EQ(render(), plain);
    line->setValue(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(render(), plain);
    line->setValue(std::numeric_limits<double>::infinity());
    EXPECT_EQ(render(), plain);

    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.yAxis()->setRange(1.0, 100.0);
    const QImage plainLog = render();
    line->setValue(-3.0);
    EXPECT_EQ(render(), plainLog);
    line->setValue(10.0);
    EXPECT_NE(render(), plainLog);
}

TEST_F(ReferenceLineTest, TheLabelSitsBesideTheLine)
{
    ReferenceLine* line     = m_plot.addHorizontalLine(5.0);
    const QImage   unnamed  = render();
    const QRect    plot     = m_plot.plotArea().toRect();
    const int      row      = pixelAt(0.0, 5.0).y();
    const QRect    aboveEnd = QRect(plot.right() - 60, row - 22, 55, 20);
    const QRect    belowEnd = QRect(plot.right() - 60, row + 3, 55, 20);
    const QRect    belowStart(plot.left() + 5, row + 3, 55, 20);

    line->setLabel("Limit");
    EXPECT_TRUE(differ(render(), unnamed, aboveEnd));  // above the right end by default
    EXPECT_FALSE(differ(render(), unnamed, belowEnd));
    line->setLabelAlignment(Qt::AlignLeft | Qt::AlignBottom);
    EXPECT_FALSE(differ(render(), unnamed, aboveEnd));
    EXPECT_TRUE(differ(render(), unnamed, belowStart));
}

TEST_F(ReferenceLineTest, TheLabelChangesSideWhereItDoesNotFit)
{
    // Too near the top of the plot for a label above it.
    ReferenceLine* line    = m_plot.addHorizontalLine(9.9);
    const QImage   unnamed = render();
    const QRect    plot    = m_plot.plotArea().toRect();
    const int      row     = pixelAt(0.0, 9.9).y();
    line->setLabel("Limit");
    EXPECT_TRUE(differ(render(), unnamed, QRect(plot.right() - 60, row + 3, 55, 20)));

    // A vertical line at the right edge: its label goes to its left.
    ReferenceLine* edge       = m_plot.addVerticalLine(9.9);
    const QImage   unnamedTwo = render();
    const int      column     = pixelAt(9.9, 0.0).x();
    edge->setLabel("End");
    EXPECT_TRUE(differ(render(), unnamedTwo, QRect(column - 40, plot.top() + 4, 36, 22)));
    EXPECT_FALSE(differ(render(), unnamedTwo, QRect(column + 2, plot.top(), 30, 60)));
}

TEST_F(ReferenceLineTest, OnTheSecondaryAxis)
{
    m_plot.yAxis2()->setRange(0.0, 100.0);
    ReferenceLine* line = m_plot.addHorizontalLine(50.0);
    line->setYAxis(m_plot.yAxis2());
    line->setColor(Qt::red);
    line->setLineStyle(Qt::SolidLine);
    line->setLineWidth(3.0);
    // Half way up the right axis is where 5 is on the left one.
    EXPECT_EQ(render().pixelColor(pixelAt(5.0, 5.0)), QColor(Qt::red));
}

}  // namespace
