#include "rocketplot/ColorScale.h"

#include <QApplication>
#include <QColor>
#include <QGradientStops>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <QString>
#include <QWheelEvent>
#include <Qt>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::ColorMap;
using rocketplot::ColorScale;
using rocketplot::PlotWidget;
using rocketplot::Range;
using rocketplot::ScatterSeries;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Three points colored by 0, 5 and 10.
ScatterSeries* addColored(PlotWidget& plot)
{
    ScatterSeries* points = plot.addScatter(std::vector{1.0, 2.0, 3.0}, std::vector{1.0, 2.0, 3.0});
    points->setColorValues(std::vector{0.0, 5.0, 10.0});
    return points;
}

bool near(const QColor& a, const QColor& b, int tolerance)
{
    return std::abs(a.red() - b.red()) <= tolerance &&
           std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

TEST(ColorScale, PointsTakeTheColorsOfTheirValues)
{
    PlotWidget           plot;
    const ScatterSeries* points = addColored(plot);
    const ColorScale&    scale  = *plot.colorScale();
    EXPECT_TRUE(scale.autoscale());
    EXPECT_EQ(scale.range(), (Range{.min = 0.0, .max = 10.0}));
    EXPECT_EQ(points->pointColor(1), scale.colorAt(5.0));
    EXPECT_EQ(scale.colorAt(0.0), QColor(0x44, 0x01, 0x54));  // viridis
    EXPECT_EQ(scale.colorAt(10.0), QColor(0xfd, 0xe7, 0x25));
    EXPECT_EQ(scale.colorAt(-3.0), scale.colorAt(0.0));  // past an end: its color
    EXPECT_EQ(scale.colorAt(kNaN), plot.theme().secondaryText);
    EXPECT_EQ(points->colorValue(2), 10.0);
}

TEST(ColorScale, AFixedRangeRecolors)
{
    PlotWidget           plot;
    const ScatterSeries* points = addColored(plot);
    ColorScale&          scale  = *plot.colorScale();
    const QSignalSpy     ranged(&scale, &ColorScale::rangeChanged);
    scale.setRange(5.0, 10.0);
    EXPECT_FALSE(scale.autoscale());
    EXPECT_EQ(ranged.count(), 1);
    EXPECT_EQ(points->pointColor(1), scale.colorAt(5.0));
    EXPECT_EQ(points->pointColor(1), QColor(0x44, 0x01, 0x54));  // now the bottom
    // More data doesn't move it; autoscale on fits it again.
    plot.addScatter(std::vector{1.0}, std::vector{1.0})->setColorValues(std::vector{-20.0});
    EXPECT_EQ(scale.min(), 5.0);
    scale.setAutoscale(true);
    EXPECT_EQ(scale.range(), (Range{.min = -20.0, .max = 10.0}));  // both series
}

TEST(ColorScale, MapsAndGradients)
{
    PlotWidget  plot;
    ColorScale& scale = *plot.colorScale();
    addColored(plot);
    scale.setColorMap(ColorMap::BLUE_RED);
    EXPECT_EQ(scale.colorAt(5.0), QColor(Qt::white));
    scale.setGradient({{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::green)}});
    EXPECT_EQ(scale.colorMap(), ColorMap::CUSTOM);
    EXPECT_TRUE(near(scale.colorAt(5.0), QColor(0, 128, 0), 1));
    scale.resetColorMap();
    EXPECT_EQ(scale.colorMap(), ColorMap::VIRIDIS);
}

TEST(ColorScale, TheKeyShowsOnlyForColorValues)
{
    PlotWidget plot;
    plot.resize(600, 400);
    plot.addScatter(std::vector{1.0, 2.0}, std::vector{1.0, 2.0});
    EXPECT_FALSE(plot.colorScale()->isVisible());
    const double   right  = plot.plotArea().right();
    ScatterSeries* points = plot.addScatter(std::vector{1.0, 2.0}, std::vector{1.0, 2.0});
    points->setColorValues(std::vector{1.0, 2.0});
    EXPECT_TRUE(plot.colorScale()->isVisible());
    EXPECT_LT(plot.plotArea().right(), right - 30.0);  // room for the key
    points->setVisible(false);
    EXPECT_FALSE(plot.colorScale()->isVisible());
    plot.colorScale()->setVisible(true);
    EXPECT_TRUE(plot.colorScale()->isVisible());
}

TEST(ColorScale, ColorsAndColorValuesTakeEachOthersPlace)
{
    PlotWidget     plot;
    ScatterSeries* points = addColored(plot);
    EXPECT_THROW(points->setColorValues(std::vector{1.0}), std::invalid_argument);
    points->setColors(std::vector{QColor(Qt::red), QColor(Qt::green), QColor(Qt::blue)});
    EXPECT_FALSE(points->hasColorValues());
    EXPECT_EQ(points->pointColor(0), QColor(Qt::red));
    points->setColorValues(std::vector{1.0, 2.0, 3.0});
    EXPECT_FALSE(points->hasColors());
    // Both go with the data.
    points->setData(std::vector{1.0}, std::vector{1.0});
    EXPECT_FALSE(points->hasColorValues());
    EXPECT_TRUE(std::isnan(points->colorValue(0)));
}

TEST(ColorScale, ItsSettingsAreSaved)
{
    PlotWidget  plot;
    ColorScale& scale = *plot.colorScale();
    scale.setRange(-1.0, 1.0);
    scale.setColorMap(ColorMap::COOL_WARM);
    scale.setNanColor(Qt::magenta);
    scale.setNumberFormat(rocketplot::NumberFormat::SI);
    scale.setVisible(false);
    scale.setLabel("not saved");
    PlotWidget restored;
    ASSERT_TRUE(restored.restoreState(plot.saveState()));
    const ColorScale& back = *restored.colorScale();
    EXPECT_EQ(back.range(), (Range{.min = -1.0, .max = 1.0}));
    EXPECT_FALSE(back.autoscale());
    EXPECT_EQ(back.colorMap(), ColorMap::COOL_WARM);
    EXPECT_EQ(back.nanColor(), QColor(Qt::magenta));
    EXPECT_EQ(back.numberFormat(), rocketplot::NumberFormat::SI);
    EXPECT_FALSE(back.isVisible());
    EXPECT_TRUE(back.label().isEmpty());
}

TEST(ColorScale, TheCsvHasTheValues)
{
    PlotWidget plot;
    plot.addScatter(std::vector{1.0, 2.0}, std::vector{3.0, 4.0}, "Landings")
        ->setColorValues(std::vector{7.5, kNaN});
    plot.colorScale()->setLabel("Wind (m/s)");
    EXPECT_EQ(plot.toCsv(), "x,Landings,Landings (Wind (m/s))\n1,3,7.5\n2,4,\n");
}

class ColorKeyTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(600, 400);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        addColored(m_plot);
        m_plot.colorScale()->setLabel("Wind (m/s)");
    }

    PlotWidget m_plot;
};

TEST_F(ColorKeyTest, TheBarRunsFromTheFirstColorToTheLast)
{
    const QImage image = m_plot.grab().toImage();
    // The key is right of the plot area: find its bar, the first run of viridis colors there.
    const QRectF area = m_plot.plotArea();
    const int    row  = qRound(area.center().y());
    int          bar  = -1;
    for (int x = qRound(area.right()) + 4; x < image.width() && bar < 0; ++x)
    {
        if (near(image.pixelColor(x, row), m_plot.colorScale()->colorAt(5.0), 12))
        {
            bar = x;
        }
    }
    ASSERT_GE(bar, 0);
    const int bottom = static_cast<int>(std::floor(area.bottom())) - 1;
    const int top    = static_cast<int>(std::ceil(area.top()));
    EXPECT_TRUE(near(image.pixelColor(bar + 3, bottom), QColor(0x44, 0x01, 0x54), 2))
        << image.pixelColor(bar + 3, bottom).name().toStdString();
    EXPECT_TRUE(near(image.pixelColor(bar + 3, top), QColor(0xfd, 0xe7, 0x25), 2))
        << image.pixelColor(bar + 3, top).name().toStdString();
}

TEST_F(ColorKeyTest, AnExportLooksLikeTheWidget)
{
    EXPECT_EQ(m_plot.renderToImage().convertToFormat(QImage::Format_RGB32),
              m_plot.grab().toImage().convertToFormat(QImage::Format_RGB32));
}

TEST_F(ColorKeyTest, AWheelOverTheKeyIsNoZoom)
{
    m_plot.addLine(std::vector{1.0, 2.0}, std::vector{1.0, 2.0})->setYAxis(m_plot.yAxis2());
    static_cast<void>(m_plot.grab());
    const Range   y2   = m_plot.yAxis2()->range();
    const QRectF  area = m_plot.plotArea();
    const QPointF over(m_plot.width() - 30.0, area.center().y());  // on the key's labels
    QWheelEvent   event(over, m_plot.mapToGlobal(over), QPoint(), QPoint(0, 120), Qt::NoButton,
                        Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&m_plot, &event);
    EXPECT_EQ(m_plot.yAxis2()->range(), y2);
}

}  // namespace
