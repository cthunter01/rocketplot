#include "rocketplot/Axis.h"

#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <cmath>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"

namespace
{

using rocketplot::Theme;

class AxisTest : public rocketplot::test::RenderedPlotTest
{
protected:
    // How many pixels of @p area in @p image aren't the background.
    [[nodiscard]] static int inked(const QImage& image, QRect area)
    {
        area      = area.intersected(image.rect());
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            for (int x = area.left(); x <= area.right(); ++x)
            {
                count += image.pixelColor(x, y) != Theme::light().background ? 1 : 0;
            }
        }
        return count;
    }

    // What lies below the plot area, past the tick marks: the x tick labels.
    [[nodiscard]] QRect belowTheTicks() const
    {
        const QRectF area = m_plot.plotArea();
        const int top = static_cast<int>(std::ceil(area.bottom() + Theme::light().tickLength)) + 2;
        return {0, top, m_plot.width(), m_plot.height() - top};
    }

    // What lies left of the plot area, past the tick marks: the y tick labels.
    [[nodiscard]] QRect leftOfTheTicks() const
    {
        const QRectF area = m_plot.plotArea();
        const int right = static_cast<int>(std::floor(area.left() - Theme::light().tickLength)) - 3;
        return {0, static_cast<int>(area.top()), right, static_cast<int>(area.height())};
    }
};

TEST_F(AxisTest, WritesItsTickLabelsUnlessToldNotTo)
{
    EXPECT_TRUE(m_plot.xAxis()->areTickLabelsVisible());
    EXPECT_GT(inked(render(), belowTheTicks()), 0);
    EXPECT_GT(inked(render(), leftOfTheTicks()), 0);

    const QSignalSpy changed(m_plot.xAxis(), &rocketplot::Axis::changed);
    m_plot.xAxis()->setTickLabelsVisible(false);
    m_plot.yAxis()->setTickLabelsVisible(false);

    EXPECT_EQ(changed.count(), 1);
    EXPECT_FALSE(m_plot.xAxis()->areTickLabelsVisible());
    EXPECT_EQ(inked(render(), belowTheTicks()), 0);
    EXPECT_EQ(inked(render(), leftOfTheTicks()), 0);
}

TEST_F(AxisTest, WithoutTickLabelsThePlotAreaTakesTheirRoom)
{
    const QRectF labeled = m_plot.plotArea();

    m_plot.xAxis()->setTickLabelsVisible(false);
    const QRectF withoutX = m_plot.plotArea();
    m_plot.yAxis()->setTickLabelsVisible(false);
    const QRectF withoutBoth = m_plot.plotArea();

    EXPECT_GT(withoutX.bottom(), labeled.bottom() + 5.0);
    EXPECT_DOUBLE_EQ(withoutX.left(), labeled.left());
    EXPECT_LT(withoutBoth.left(), labeled.left() - 5.0);
}

TEST_F(AxisTest, WithoutTickLabelsTheAxisKeepsItsLineTicksAndGrid)
{
    const QImage labeled = render();
    const QRect  grid    = m_plot.plotArea().toRect().adjusted(2, 2, -2, -2);
    ASSERT_GT(inked(labeled, grid), 0);  // the grid lines

    m_plot.xAxis()->setTickLabelsVisible(false);
    const QImage plain = render();

    const QRectF area = m_plot.plotArea();
    EXPECT_GT(inked(plain, m_plot.plotArea().toRect().adjusted(2, 2, -2, -2)), 0);
    // The axis line and its tick marks: the rows of pixels right below the plot area.
    const QRect ticks(static_cast<int>(area.left()), static_cast<int>(area.bottom()),
                      static_cast<int>(area.width()), 4);
    EXPECT_GT(inked(plain, ticks), static_cast<int>(area.width()));
}

TEST_F(AxisTest, AnAxisLabelStaysWhenTheTickLabelsGo)
{
    m_plot.xAxis()->setTickLabelsVisible(false);
    ASSERT_EQ(inked(render(), belowTheTicks()), 0);

    m_plot.xAxis()->setLabel("Time (s)");

    EXPECT_GT(inked(render(), belowTheTicks()), 0);
}

}  // namespace
