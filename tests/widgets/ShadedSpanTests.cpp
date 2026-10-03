#include "rocketplot/ShadedSpan.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSignalSpy>
#include <Qt>
#include <limits>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::Range;
using rocketplot::ShadedSpan;

using ShadedSpanTest = rocketplot::test::RenderedPlotTest;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

TEST(ShadedSpan, Defaults)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    const ShadedSpan* vertical   = plot.addVerticalSpan(6.0, 2.0, "phase");
    const ShadedSpan* horizontal = plot.addHorizontalSpan(1.0, 3.0);
    EXPECT_EQ(vertical->orientation(), Qt::Vertical);
    EXPECT_EQ(horizontal->orientation(), Qt::Horizontal);
    EXPECT_EQ(vertical->range(), (Range{.min = 2.0, .max = 6.0}));  // put in order
    EXPECT_EQ(vertical->label(), "phase");
    EXPECT_DOUBLE_EQ(vertical->opacity(), plot.theme().spanOpacity);
    EXPECT_EQ(vertical->color(), plot.theme().annotation);
    EXPECT_EQ(vertical->labelAlignment(), Qt::AlignLeft | Qt::AlignBottom);
}

TEST(ShadedSpan, SettersSignalOnlyChanges)
{
    PlotWidget       plot;
    ShadedSpan*      span = plot.addVerticalSpan(1.0, 2.0);
    const QSignalSpy changed(span, &rocketplot::Annotation::changed);
    span->setRange(1.0, 2.0);
    span->setRange(2.0, 1.0);  // the same range
    EXPECT_EQ(changed.count(), 0);
    span->setMax(5.0);
    span->setMin(7.0);
    EXPECT_EQ(span->range(), (Range{.min = 5.0, .max = 7.0}));
    span->setLabel("phase");
    span->setOpacity(3.0);  // as opaque as it gets
    span->setLabelAlignment(Qt::AlignCenter);
    EXPECT_EQ(changed.count(), 5);
    EXPECT_DOUBLE_EQ(span->opacity(), 1.0);
    span->resetOpacity();
    EXPECT_DOUBLE_EQ(span->opacity(), plot.theme().spanOpacity);
}

TEST_F(ShadedSpanTest, FillsThePlotBetweenTwoXValues)
{
    const QImage plain = render();
    ShadedSpan*  span  = m_plot.addVerticalSpan(3.0, 6.0);
    span->setColor(Qt::red);
    span->setOpacity(1.0);
    const QImage image = render();
    for (const double y : {0.1, 5.0, 9.9})
    {
        for (const double inside : {3.1, 5.9})
        {
            EXPECT_EQ(image.pixelColor(pixelAt(inside, y)), QColor(Qt::red)) << inside << ", " << y;
        }
        for (const double outside : {2.9, 6.1})
        {
            EXPECT_EQ(image.pixelColor(pixelAt(outside, y)), plain.pixelColor(pixelAt(outside, y)))
                << outside << ", " << y;
        }
    }
    // Not over the axes.
    EXPECT_EQ(image.pixelColor(pixelAt(4.5, -0.3)), plain.pixelColor(pixelAt(4.5, -0.3)));
}

TEST_F(ShadedSpanTest, FillsThePlotBetweenTwoYValues)
{
    const QImage plain = render();
    ShadedSpan*  band  = m_plot.addHorizontalSpan(3.0, 6.0);
    band->setColor(Qt::red);
    band->setOpacity(1.0);
    const QImage image = render();
    EXPECT_EQ(image.pixelColor(pixelAt(0.1, 4.5)), QColor(Qt::red));
    EXPECT_EQ(image.pixelColor(pixelAt(9.9, 4.5)), QColor(Qt::red));
    EXPECT_EQ(image.pixelColor(pixelAt(5.0, 6.5)), plain.pixelColor(pixelAt(5.0, 6.5)));
}

TEST_F(ShadedSpanTest, IsAWashOfItsColorByDefault)
{
    const QImage plain = render();
    m_plot.addVerticalSpan(3.0, 6.0);
    const QColor shaded = render().pixelColor(pixelAt(4.4, 4.4));
    EXPECT_NE(shaded, plain.pixelColor(pixelAt(4.4, 4.4)));
    EXPECT_GT(shaded.lightnessF(), 0.8F);
}

TEST_F(ShadedSpanTest, AnInfiniteEndRunsToTheEdgeOfThePlot)
{
    ShadedSpan* span = m_plot.addVerticalSpan(-kInfinity, 3.0);
    span->setColor(Qt::red);
    span->setOpacity(1.0);
    EXPECT_EQ(render().pixelColor(pixelAt(0.05, 5.0)), QColor(Qt::red));
    span->setRange(7.0, kInfinity);
    EXPECT_EQ(render().pixelColor(pixelAt(9.95, 5.0)), QColor(Qt::red));
    EXPECT_NE(render().pixelColor(pixelAt(6.5, 5.0)), QColor(Qt::red));
    // On a log axis, zero and below are off its low end.
    m_plot.xAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.xAxis()->setRange(1.0, 100.0);
    span->setRange(-5.0, 10.0);
    EXPECT_EQ(render().pixelColor(pixelAt(1.1, 5.0)), QColor(Qt::red));
    EXPECT_NE(render().pixelColor(pixelAt(20.0, 5.0)), QColor(Qt::red));
}

TEST_F(ShadedSpanTest, ASpanOutOfViewOrWithoutEndsIsNotDrawn)
{
    const QImage plain = render();
    ShadedSpan*  span  = m_plot.addVerticalSpan(20.0, 30.0, "later");
    EXPECT_EQ(render(), plain);
    span->setRange(std::numeric_limits<double>::quiet_NaN(), 5.0);
    EXPECT_EQ(render(), plain);
    span->setRange(4.0, 4.0);  // nothing between its ends
    EXPECT_EQ(render(), plain);
}

TEST_F(ShadedSpanTest, TheLabelIsInsideIt)
{
    ShadedSpan*  span    = m_plot.addVerticalSpan(3.0, 9.0);
    const QImage unnamed = render();
    const QRect  plot    = m_plot.plotArea().toRect();
    const int    left    = pixelAt(3.0, 0.0).x();
    const QRect  bottomLeft(left + 2, plot.bottom() - 22, 60, 20);
    const QRect  topLeft(left + 2, plot.top() + 2, 60, 20);
    span->setLabel("Phase");
    EXPECT_TRUE(differ(render(), unnamed, bottomLeft));
    EXPECT_FALSE(differ(render(), unnamed, topLeft));
    span->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    EXPECT_TRUE(differ(render(), unnamed, topLeft));

    // With its start out of view, the label stays at the edge of what is in view.
    span->setRange(-50.0, 9.0);
    span->setLabel({});
    const QImage unnamedWide = render();
    span->setLabel("Phase");
    EXPECT_TRUE(differ(render(), unnamedWide, QRect(plot.left() + 2, plot.top() + 2, 60, 20)));
}

}  // namespace
