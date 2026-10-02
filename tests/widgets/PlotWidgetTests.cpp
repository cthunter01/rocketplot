#include "rocketplot/PlotWidget.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPalette>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QWheelEvent>
#include <Qt>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::Range;

class PlotWidgetTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        m_plot.addLine(std::vector<double>{0, 5, 10}, std::vector<double>{-5, 5, 0}, "first");
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    }

    void wheel(QPointF position, int units, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QWheelEvent event(position, m_plot.mapToGlobal(position), QPoint(), QPoint(0, units),
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&m_plot, &event);
    }

    [[nodiscard]] QPointF plotCenter() const { return m_plot.plotArea().center(); }

    PlotWidget m_plot;
};

TEST_F(PlotWidgetTest, AutoscaleFitsTheDataWithAMargin)
{
    const Range x = m_plot.xAxis()->range();
    const Range y = m_plot.yAxis()->range();
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    EXPECT_DOUBLE_EQ(x.min, -10.0 * m_plot.xAxis()->autoscaleMargin());
    EXPECT_DOUBLE_EQ(x.max, 10.0 + (10.0 * m_plot.xAxis()->autoscaleMargin()));
    EXPECT_LT(y.min, -5.0);
    EXPECT_GT(y.max, 5.0);
}

TEST_F(PlotWidgetTest, AutoscaleFollowsNewData)
{
    m_plot.addLine(std::vector<double>{0, 100}, std::vector<double>{0, 0});
    EXPECT_GT(m_plot.xAxis()->max(), 100.0);
}

TEST_F(PlotWidgetTest, AutoscaleIgnoresHiddenSeriesAndNaN)
{
    auto* far = m_plot.addLine(std::vector<double>{0, 1000}, std::vector<double>{0, 0});
    far->setVisible(false);
    EXPECT_LT(m_plot.xAxis()->max(), 11.0);
    m_plot.addLine(std::vector<double>{0, std::numeric_limits<double>::quiet_NaN()},
                   std::vector<double>{0, 1e9});
    EXPECT_LT(m_plot.yAxis()->max(), 1e8);
}

TEST_F(PlotWidgetTest, WheelZoomsAboutThePointer)
{
    const QPointF position(plotCenter().x() - 100.0, plotCenter().y() + 50.0);
    const QPointF before = m_plot.mapToData(position);
    const Range   x      = m_plot.xAxis()->range();
    const Range   y      = m_plot.yAxis()->range();
    wheel(position, 120);  // one notch in
    EXPECT_NEAR(m_plot.xAxis()->range().span(), x.span() * std::pow(2.0, -0.25), 1e-9);
    EXPECT_NEAR(m_plot.yAxis()->range().span(), y.span() * std::pow(2.0, -0.25), 1e-9);
    const QPointF after = m_plot.mapToData(position);
    EXPECT_NEAR(after.x(), before.x(), 1e-9);
    EXPECT_NEAR(after.y(), before.y(), 1e-9);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_FALSE(m_plot.yAxis()->autoscale());
}

TEST_F(PlotWidgetTest, WheelOverAnAxisZoomsOnlyThatAxis)
{
    const QRectF area = m_plot.plotArea();
    const Range  y    = m_plot.yAxis()->range();
    wheel(QPointF(area.center().x(), area.bottom() + 10.0), 120);  // over the x axis
    EXPECT_EQ(m_plot.yAxis()->range(), y);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
    EXPECT_FALSE(m_plot.xAxis()->autoscale());

    const Range x = m_plot.xAxis()->range();
    wheel(QPointF(area.left() - 10.0, area.center().y()), -120);  // over the y axis, out
    EXPECT_EQ(m_plot.xAxis()->range(), x);
    EXPECT_GT(m_plot.yAxis()->range().span(), y.span());
}

TEST_F(PlotWidgetTest, ModifiersPickTheAxis)
{
    const Range x = m_plot.xAxis()->range();
    const Range y = m_plot.yAxis()->range();
    wheel(plotCenter(), 120, Qt::ControlModifier);
    EXPECT_LT(m_plot.xAxis()->range().span(), x.span());
    EXPECT_EQ(m_plot.yAxis()->range(), y);

    const Range x2 = m_plot.xAxis()->range();
    wheel(plotCenter(), 120, Qt::ShiftModifier);
    EXPECT_EQ(m_plot.xAxis()->range(), x2);
    EXPECT_LT(m_plot.yAxis()->range().span(), y.span());
}

TEST_F(PlotWidgetTest, DragPans)
{
    const QRectF area = m_plot.plotArea();
    const Range  x    = m_plot.xAxis()->range();
    const Range  y    = m_plot.yAxis()->range();
    const QPoint start(static_cast<int>(area.center().x()), static_cast<int>(area.center().y()));
    QTest::mousePress(&m_plot, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&m_plot, start + QPoint(100, 0));
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, start + QPoint(100, 0));
    // Content dragged right by 100 px: the view moved left by 100 px worth of data.
    EXPECT_NEAR(m_plot.xAxis()->min(), x.min - (100.0 * x.span() / area.width()), 1e-9);
    EXPECT_NEAR(m_plot.xAxis()->range().span(), x.span(), 1e-9);
    EXPECT_NEAR(m_plot.yAxis()->min(), y.min, 1e-9);
}

TEST_F(PlotWidgetTest, DoubleClickReturnsToAutoscale)
{
    const Range x = m_plot.xAxis()->range();
    wheel(plotCenter(), 240);
    ASSERT_NE(m_plot.xAxis()->range(), x);
    const QPoint center(static_cast<int>(plotCenter().x()), static_cast<int>(plotCenter().y()));
    QTest::mouseDClick(&m_plot, Qt::LeftButton, Qt::NoModifier, center);
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
    EXPECT_EQ(m_plot.xAxis()->range(), x);
}

TEST_F(PlotWidgetTest, SetRangeTurnsAutoscaleOffAndSignals)
{
    const QSignalSpy view(&m_plot, &PlotWidget::viewChanged);
    m_plot.xAxis()->setRange(2.0, 4.0);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_EQ(m_plot.xAxis()->range(), (Range{.min = 2.0, .max = 4.0}));
    EXPECT_EQ(view.count(), 1);
    m_plot.xAxis()->setRange(5.0, 5.0);  // can't be shown: ignored
    EXPECT_EQ(m_plot.xAxis()->range(), (Range{.min = 2.0, .max = 4.0}));
    m_plot.xAxis()->setRange(9.0, 7.0);  // reversed: swapped
    EXPECT_EQ(m_plot.xAxis()->range(), (Range{.min = 7.0, .max = 9.0}));
}

TEST_F(PlotWidgetTest, MapToDataAndBack)
{
    const QPointF data(3.0, 1.0);
    const QPointF widget = m_plot.mapFromData(data);
    EXPECT_TRUE(m_plot.plotArea().contains(widget));
    const QPointF back = m_plot.mapToData(widget);
    EXPECT_NEAR(back.x(), 3.0, 1e-9);
    EXPECT_NEAR(back.y(), 1.0, 1e-9);
}

TEST_F(PlotWidgetTest, RendersBackgroundInThemeColor)
{
    const QImage image = m_plot.grab().toImage();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.pixelColor(2, 2), m_plot.theme().background);
    m_plot.setThemeMode(rocketplot::ThemeMode::DARK);
    EXPECT_EQ(m_plot.grab().toImage().pixelColor(2, 2), rocketplot::Theme::dark().background);
}

TEST_F(PlotWidgetTest, RendersAwkwardData)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    m_plot.addLine(std::vector<double>{});
    m_plot.addLine(std::vector<double>{nan, nan, nan}, "all NaN");
    m_plot.addScatter(std::vector<double>{1, 2}, std::vector<double>{nan, 3}, "scatter");
    m_plot.addLine(std::vector<double>{3, 1, 2, nan, 0}, std::vector<double>{1, 2, 3, 4, 5},
                   "unsorted");
    m_plot.setTitle("Awkward");
    m_plot.xAxis()->setLabel("x");
    m_plot.yAxis()->setLabel("y");
    m_plot.setDebugOverlay(true);
    EXPECT_FALSE(m_plot.grab().isNull());
    m_plot.resize(20, 20);  // too small to plot in
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, ExtremeRangesRender)
{
    m_plot.xAxis()->setRange(1.7e9, 1.7e9 + 0.001);
    m_plot.yAxis()->setRange(-1e200, 1e200);
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, RemovingSeriesKeepsTheOthersColors)
{
    auto*            second = m_plot.addLine(std::vector<double>{1}, "second");
    const auto*      third  = m_plot.addLine(std::vector<double>{1}, "third");
    const QColor     color  = third->color();
    const QSignalSpy removed(&m_plot, &PlotWidget::seriesRemoved);
    m_plot.removeSeries(second);
    EXPECT_EQ(removed.count(), 1);
    EXPECT_EQ(m_plot.series().size(), 2);
    EXPECT_EQ(third->color(), color);
    m_plot.clearSeries();
    EXPECT_TRUE(m_plot.series().isEmpty());
}

TEST_F(PlotWidgetTest, SystemThemeFollowsThePalette)
{
    m_plot.setThemeMode(rocketplot::ThemeMode::SYSTEM);
    QPalette dark = m_plot.palette();
    dark.setColor(QPalette::Base, QColor(0x20, 0x20, 0x20));
    const QSignalSpy changed(&m_plot, &PlotWidget::themeChanged);
    m_plot.setPalette(dark);
    EXPECT_GE(changed.count(), 1);
    EXPECT_EQ(m_plot.theme().background, QColor(0x20, 0x20, 0x20));
    EXPECT_EQ(m_plot.theme().seriesColors, rocketplot::Theme::dark().seriesColors);
}

TEST_F(PlotWidgetTest, CustomTheme)
{
    rocketplot::Theme theme = rocketplot::Theme::light();
    theme.background        = Qt::yellow;
    m_plot.setTheme(theme);
    EXPECT_EQ(m_plot.themeMode(), rocketplot::ThemeMode::CUSTOM);
    EXPECT_EQ(m_plot.grab().toImage().pixelColor(2, 2), QColor(Qt::yellow));
}

TEST_F(PlotWidgetTest, SecondaryAxisFitsItsOwnSeries)
{
    const double rightBefore = m_plot.plotArea().right();
    auto* speed = m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 5000}, "speed");
    EXPECT_GT(m_plot.yAxis()->max(), 5000.0);  // on the left axis for now
    speed->setYAxis(m_plot.yAxis2());
    EXPECT_TRUE(speed->isOnSecondaryYAxis());
    EXPECT_LT(m_plot.yAxis()->max(), 10.0);
    EXPECT_GT(m_plot.yAxis2()->max(), 5000.0);
    EXPECT_LT(m_plot.plotArea().right(), rightBefore);  // room for the right axis' labels
    m_plot.setTitle(QStringLiteral("two axes"));
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, WheelOverTheRightAxisZoomsOnlyIt)
{
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 5000})
        ->setYAxis(m_plot.yAxis2());
    const QRectF area = m_plot.plotArea();
    const Range  x    = m_plot.xAxis()->range();
    const Range  y    = m_plot.yAxis()->range();
    const Range  y2   = m_plot.yAxis2()->range();
    wheel(QPointF(area.right() + 10.0, area.center().y()), 120);
    EXPECT_EQ(m_plot.xAxis()->range(), x);
    EXPECT_EQ(m_plot.yAxis()->range(), y);
    EXPECT_LT(m_plot.yAxis2()->range().span(), y2.span());
}

TEST_F(PlotWidgetTest, MapToDataOnTheRightAxis)
{
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{100, 200})
        ->setYAxis(m_plot.yAxis2());
    const QPointF widget = m_plot.mapFromData(QPointF(5.0, 150.0), m_plot.yAxis2());
    EXPECT_NEAR(m_plot.mapToData(widget, m_plot.yAxis2()).y(), 150.0, 1e-9);
}

TEST_F(PlotWidgetTest, LogScaleFitsThePositiveData)
{
    m_plot.clearSeries();
    m_plot.addLine(std::vector<double>{1, 2, 3, 4}, std::vector<double>{-1, 0.01, 10, 1000});
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_GT(m_plot.yAxis()->min(), 0.0);
    EXPECT_LT(m_plot.yAxis()->min(), 0.01);
    EXPECT_GT(m_plot.yAxis()->max(), 1000.0);
    m_plot.yAxis()->setRange(-1.0, 10.0);  // can't be shown on a log axis
    EXPECT_GT(m_plot.yAxis()->min(), 0.0);
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, SwitchingToLogWithANonPositiveRangeRefits)
{
    m_plot.yAxis()->setRange(-10.0, 10.0);
    EXPECT_FALSE(m_plot.yAxis()->autoscale());
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
    EXPECT_GT(m_plot.yAxis()->min(), 0.0);
}

TEST_F(PlotWidgetTest, FitVisibleFollowsTheXRange)
{
    m_plot.clearSeries();
    std::vector<double> y(1000);
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        y[i] = static_cast<double>(i);  // y = x
    }
    m_plot.addLine(y);
    m_plot.yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    m_plot.xAxis()->setRange(100.0, 200.0);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
    EXPECT_LT(m_plot.yAxis()->min(), 100.0);
    EXPECT_GT(m_plot.yAxis()->min(), 90.0);
    EXPECT_GT(m_plot.yAxis()->max(), 200.0);
    EXPECT_LT(m_plot.yAxis()->max(), 210.0);
}

TEST_F(PlotWidgetTest, FollowLatestScrollsWithAppendedData)
{
    m_plot.clearSeries();
    auto* live = m_plot.addLine(std::vector<double>{}, std::vector<double>{});
    m_plot.xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
    m_plot.xAxis()->setFollowWindow(10.0);
    for (int i = 0; i <= 100; ++i)
    {
        live->append(static_cast<double>(i) * 0.5, 1.0);  // up to t = 50
    }
    EXPECT_NEAR(m_plot.xAxis()->range().span(), 10.0, 1e-9);
    EXPECT_GT(m_plot.xAxis()->max(), 50.0);
    EXPECT_LT(m_plot.xAxis()->min(), 50.0);
    EXPECT_GT(m_plot.xAxis()->min(), 39.0);
}

TEST_F(PlotWidgetTest, DateTimeAxisRenders)
{
    m_plot.clearSeries();
    const double start = 1.7e9;
    m_plot.addLine(std::vector<double>{start, start + 3600.0, start + 7200.0},
                   std::vector<double>{1, 3, 2});
    m_plot.xAxis()->setScaleType(rocketplot::ScaleType::DATE_TIME);
    m_plot.xAxis()->setTimeZone(QTimeZone(QByteArrayLiteral("Europe/Berlin")));
    EXPECT_FALSE(m_plot.grab().isNull());
    m_plot.xAxis()->setRange(start, start + 0.01);  // ten milliseconds
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, RichTextLabels)
{
    m_plot.setTitle(QStringLiteral("v<sub>z</sub> at T<sub>0</sub>"));
    m_plot.xAxis()->setLabel(QStringLiteral("<i>t</i> (s)"));
    m_plot.yAxis()->setLabel(QStringLiteral("v<sub>z</sub> (m/s)"));
    m_plot.series().front()->setName(QStringLiteral("<b>first</b>"));
    m_plot.legend()->setVisible(true);
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST_F(PlotWidgetTest, ResetViewTurnsAutoscaleOnEverywhere)
{
    m_plot.xAxis()->setRange(1.0, 2.0);
    m_plot.yAxis()->setRange(1.0, 2.0);
    m_plot.yAxis2()->setRange(1.0, 2.0);
    m_plot.resetView();
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
    EXPECT_TRUE(m_plot.yAxis2()->autoscale());
}

}  // namespace
