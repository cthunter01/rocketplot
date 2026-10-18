// A fixed aspect ratio: PlotWidget::aspectRatio.

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <QSize>
#include <QTest>
#include <QWheelEvent>
#include <Qt>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotLink.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/RectAnnotation.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::Range;

// How far @p plot is from its aspect ratio: 0 when a unit of y takes ratio times the pixels of a
// unit of x.
double aspectError(const PlotWidget& plot)
{
    const QRectF area      = plot.plotArea();
    const double xPerPixel = plot.xAxis()->range().span() / area.width();
    const double yPerPixel = plot.yAxis()->range().span() / area.height();
    return std::abs(xPerPixel - (plot.aspectRatio() * yPerPixel)) / xPerPixel;
}

bool holds(Range outer, Range inner)
{
    return outer.min <= inner.min && outer.max >= inner.max;
}

// A plot on screen of points from (0, 0) to (10, 5), to scale.
class AspectTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(600, 400);
        m_plot.addScatter(std::vector<double>{0, 10, 3}, std::vector<double>{0, 5, 4});
        m_plot.setAspectRatio(1.0);
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    }

    void wheel(QPointF position, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QWheelEvent event(position, m_plot.mapToGlobal(position), QPoint(), QPoint(0, 240),
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&m_plot, &event);
    }

    void boxZoom(QPoint from, QPoint to)
    {
        QTest::mousePress(&m_plot, Qt::LeftButton, Qt::ShiftModifier, from);
        QTest::mouseMove(&m_plot, (from + to) / 2);
        QTest::mouseMove(&m_plot, to);
        QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::ShiftModifier, to);
    }

    // Expects the plot to be to scale, showing all of @p x and @p y about their middles.
    void expectToScaleShowing(Range x, Range y) const
    {
        EXPECT_LT(aspectError(m_plot), 1e-9);
        EXPECT_TRUE(holds(m_plot.xAxis()->range(), x));
        EXPECT_TRUE(holds(m_plot.yAxis()->range(), y));
        EXPECT_DOUBLE_EQ(m_plot.xAxis()->range().center(), x.center());
        EXPECT_DOUBLE_EQ(m_plot.yAxis()->range().center(), y.center());
    }

    // Expects a wheel turn at @p position to zoom y in, to scale.
    void expectWheelZoomsY(QPointF position, Qt::KeyboardModifiers modifiers)
    {
        const Range before = m_plot.yAxis()->range();
        wheel(position, modifiers);
        EXPECT_LT(aspectError(m_plot), 1e-9) << modifiers;
        EXPECT_LT(m_plot.yAxis()->range().span(), before.span()) << modifiers;
    }

    PlotWidget m_plot;
};

TEST_F(AspectTest, AutoscaleShowsAllTheDataToScale)
{
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_TRUE(holds(m_plot.xAxis()->range(), {.min = 0.0, .max = 10.0}));
    EXPECT_TRUE(holds(m_plot.yAxis()->range(), {.min = 0.0, .max = 5.0}));
    // Still autoscaling: more data is fitted, to scale.
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    m_plot.addScatter(std::vector<double>{-20}, std::vector<double>{30});
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_TRUE(holds(m_plot.yAxis()->range(), {.min = 0.0, .max = 30.0}));
}

TEST_F(AspectTest, AResizeKeepsTheRegionAskedForInView)
{
    const Range x{.min = 2.0, .max = 4.0};
    const Range y{.min = 1.0, .max = 2.0};
    m_plot.xAxis()->setRange(x);
    m_plot.yAxis()->setRange(y);
    for (const QSize size : {QSize(300, 500), QSize(900, 300), QSize(600, 400)})
    {
        const testing::ScopedTrace trace(__FILE__, __LINE__, size.width());
        m_plot.resize(size);
        expectToScaleShowing(x, y);
    }
    // One of them is just what was asked for.
    EXPECT_TRUE(m_plot.xAxis()->range() == x || m_plot.yAxis()->range() == y);
}

TEST_F(AspectTest, ToScaleAtEveryWidth)
{
    // At some widths the ranges to scale have wider y labels than those to scale in the narrower
    // plot area that leaves, and those labels leave the wider one: it keeps the wider labels' room.
    for (int width = 300; width <= 1000; width += 2)
    {
        m_plot.resize(width, 400);
        EXPECT_LT(aspectError(m_plot), 1e-9) << width;
    }
}

TEST_F(AspectTest, WheelZoomsBothAxes)
{
    const Range x = m_plot.xAxis()->range();
    wheel(m_plot.plotArea().center());
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_LT(m_plot.xAxis()->range().span(), x.span());
}

TEST_F(AspectTest, WheelsThatZoomOneAxisZoomBoth)
{
    // Ctrl + wheel would zoom x alone; Shift + wheel y alone; over an axis, that axis alone.
    const QPointF middle = m_plot.plotArea().center();
    expectWheelZoomsY(middle, Qt::ControlModifier);
    expectWheelZoomsY(middle, Qt::ShiftModifier);
    expectWheelZoomsY(QPointF(middle.x(), m_plot.plotArea().bottom() + 10.0), Qt::NoModifier);
}

TEST_F(AspectTest, ABoxZoomShowsAllOfTheBox)
{
    const QRectF  area     = m_plot.plotArea();
    const QPoint  from     = (area.topLeft() + QPointF(40.0, 30.0)).toPoint();
    const QPoint  to       = (area.topLeft() + QPointF(240.0, 90.0)).toPoint();  // wider than tall
    const QPointF dataFrom = m_plot.mapToData(from);
    const QPointF dataTo   = m_plot.mapToData(to);
    boxZoom(from, to);
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_NEAR(m_plot.xAxis()->min(), dataFrom.x(), 1e-9);  // just the box's width
    EXPECT_NEAR(m_plot.xAxis()->max(), dataTo.x(), 1e-9);
    EXPECT_TRUE(holds(m_plot.yAxis()->range(), {.min = dataTo.y(), .max = dataFrom.y()}));
    EXPECT_FALSE(m_plot.yAxis()->autoscale());
}

TEST_F(AspectTest, AThinBoxZoomsItsAxisAndTheOtherFollows)
{
    const QRectF area = m_plot.plotArea();
    const QPoint from = (area.topLeft() + QPointF(100.0, 100.0)).toPoint();
    const QPoint to   = from + QPoint(100, 2);  // x only
    const double left = m_plot.mapToData(from).x();
    const double y    = m_plot.yAxis()->range().center();
    boxZoom(from, to);
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_NEAR(m_plot.xAxis()->min(), left, 1e-9);
    EXPECT_NEAR(m_plot.yAxis()->range().center(), y, 1e-9);
}

TEST_F(AspectTest, ZoomFromCodeZoomsBothAxes)
{
    const Range y = m_plot.yAxis()->range();
    m_plot.zoom(2.0, Qt::Horizontal);
    EXPECT_LT(aspectError(m_plot), 1e-9);
    // Half of y, or a little more where the labels' new width left the plot area another shape.
    EXPECT_GE(m_plot.yAxis()->range().span(), (y.span() / 2.0) - 1e-9);
    EXPECT_LT(m_plot.yAxis()->range().span(), 0.55 * y.span());
    m_plot.back();
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_EQ(m_plot.yAxis()->range(), y);
}

TEST_F(AspectTest, AFitOfWhatIsVisibleFitsAllOfIt)
{
    m_plot.yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    m_plot.xAxis()->setRange(-100.0, 1.0);  // only the first point in view
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_NEAR(m_plot.yAxis()->range().center(), 2.5, 0.2);  // of all the data
}

TEST_F(AspectTest, ALogAxisIsLeftAlone)
{
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.yAxis()->setRange(1.0, 1000.0);
    m_plot.xAxis()->setRange(0.0, 1.0);
    static_cast<void>(m_plot.grab());
    EXPECT_EQ(m_plot.xAxis()->range(), (Range{.min = 0.0, .max = 1.0}));
    EXPECT_EQ(m_plot.yAxis()->range(), (Range{.min = 1.0, .max = 1000.0}));
}

TEST_F(AspectTest, ResettingItShowsWhatTheAxesWereAskedFor)
{
    m_plot.xAxis()->setRange(0.0, 1.0);
    m_plot.yAxis()->setRange(0.0, 1.0);
    ASSERT_LT(aspectError(m_plot), 1e-9);
    ASSERT_NE(m_plot.xAxis()->range(), (Range{.min = 0.0, .max = 1.0}));  // widened
    const QSignalSpy changed(&m_plot, &PlotWidget::aspectRatioChanged);
    m_plot.resetAspectRatio();
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(m_plot.aspectRatio(), 0.0);
    EXPECT_EQ(m_plot.xAxis()->range(), (Range{.min = 0.0, .max = 1.0}));
    EXPECT_EQ(m_plot.yAxis()->range(), (Range{.min = 0.0, .max = 1.0}));
}

TEST_F(AspectTest, ARatioThatIsNoneIsIgnored)
{
    for (const double ratio :
         {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        m_plot.setAspectRatio(ratio);
        EXPECT_EQ(m_plot.aspectRatio(), 1.0);
    }
}

TEST_F(AspectTest, TheRightAxisIsFree)
{
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 1000})
        ->setYAxis(m_plot.yAxis2());
    EXPECT_LT(aspectError(m_plot), 1e-9);
    EXPECT_LT(m_plot.yAxis2()->range().span(), 1100.0);  // fits its own data
}

TEST_F(AspectTest, AColorKeyKeepsTheRatio)
{
    const double right   = m_plot.plotArea().right();
    auto*        colored = m_plot.addScatter(std::vector<double>{1, 2}, std::vector<double>{1, 2});
    colored->setColorValues(std::vector<double>{0, 1});
    EXPECT_LT(m_plot.plotArea().right(), right);  // the key took room
    EXPECT_LT(aspectError(m_plot), 1e-9);
}

TEST_F(AspectTest, ItIsPartOfTheState)
{
    m_plot.setAspectRatio(2.5);
    PlotWidget restored;
    ASSERT_TRUE(restored.restoreState(m_plot.saveState()));
    EXPECT_EQ(restored.aspectRatio(), 2.5);
}

TEST_F(AspectTest, AnExportOfAnotherShapeKeepsTheRatio)
{
    // A unit square in red, then exported twice as wide as the widget.
    rocketplot::RectAnnotation* square = m_plot.addRect(QRectF(4.0, 2.0, 1.0, 1.0));
    square->setColor(Qt::red);
    square->setFillOpacity(1.0);
    const QImage image = m_plot.renderToImage({.size = QSize(1200, 400), .dpi = 96.0});
    QRect        red;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixelColor(x, y) == QColor(Qt::red))
            {
                red = red.united(QRect(x, y, 1, 1));
            }
        }
    }
    ASSERT_FALSE(red.isEmpty());
    EXPECT_NEAR(red.width(), red.height(), 1.0);
    // The widget's own axes are as they were.
    EXPECT_LT(aspectError(m_plot), 1e-9);
}

TEST(AspectLink, YFollowsALinkedXAxis)
{
    PlotWidget first;
    PlotWidget second;
    for (PlotWidget* plot : {&first, &second})
    {
        plot->resize(600, 300);
        plot->addLine(std::vector<double>{0, 10}, std::vector<double>{0, 1});
    }
    rocketplot::PlotLink link;
    link.addPlot(&first);
    link.addPlot(&second);
    second.setAspectRatio(1.0);
    first.xAxis()->setRange(2.0, 6.0);
    EXPECT_EQ(second.xAxis()->range(), (Range{.min = 2.0, .max = 6.0}));
    EXPECT_LT(aspectError(second), 1e-9);
    EXPECT_EQ(second.xAxis()->range(), (Range{.min = 2.0, .max = 6.0}));
    EXPECT_EQ(first.xAxis()->range(), (Range{.min = 2.0, .max = 6.0}));
}

}  // namespace
