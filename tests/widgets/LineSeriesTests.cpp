#include "rocketplot/LineSeries.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPen>
#include <QPoint>
#include <QRectF>
#include <QSignalSpy>
#include <Qt>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::LineSeries;
using rocketplot::PlotWidget;

// Whether @p a is near @p b, but for antialiasing.
bool isNear(const QColor& a, const QColor& b)
{
    constexpr int kTolerance = 60;
    return std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) +
               std::abs(a.blue() - b.blue()) <
           kTolerance;
}

// The lengths of the runs of pixels of @p color and of others along row @p y of @p image, from
// @p left to @p right, without the first and last (cut off by the ends).
std::vector<int> runsAlong(const QImage& image, int y, int left, int right, const QColor& color,
                           std::vector<int>& gaps)
{
    std::vector<int> dashes;
    gaps.clear();
    bool on     = isNear(image.pixelColor(left, y), color);
    int  length = 0;
    for (int x = left; x <= right; ++x)
    {
        const bool now = isNear(image.pixelColor(x, y), color);
        if (now == on)
        {
            ++length;
            continue;
        }
        (on ? dashes : gaps).push_back(length);
        on     = now;
        length = 1;
    }
    if (!dashes.empty())
    {
        dashes.erase(dashes.begin());
    }
    if (!gaps.empty())
    {
        gaps.erase(gaps.begin());
    }
    return dashes;
}

using LineSeriesTest = rocketplot::test::RenderedPlotTest;

TEST(LineSeries, Defaults)
{
    PlotWidget        plot;
    const LineSeries* line = plot.addLine(std::vector<double>{1, 2});
    EXPECT_TRUE(line->dashPattern().isEmpty());
    EXPECT_EQ(line->capStyle(), Qt::RoundCap);
    EXPECT_FALSE(line->areMarkersAlwaysVisible());
    EXPECT_TRUE(line->isMarkerFilled());
    EXPECT_DOUBLE_EQ(line->markerRingWidth(), plot.theme().markerRingWidth);
    EXPECT_EQ(line->pen().style(), Qt::SolidLine);
}

TEST(LineSeries, PenRoundTrip)
{
    PlotWidget       plot;
    LineSeries*      line = plot.addLine(std::vector<double>{1});
    const QSignalSpy spy(line, &rocketplot::Series::changed);
    line->setPen(QPen(QColor(Qt::red), 3.5, Qt::DashLine, Qt::FlatCap));
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(line->color(), QColor(Qt::red));
    EXPECT_DOUBLE_EQ(line->lineWidth(), 3.5);
    EXPECT_EQ(line->lineStyle(), Qt::DashLine);
    EXPECT_EQ(line->pen().capStyle(), Qt::FlatCap);
    line->resetColor();
    EXPECT_EQ(line->color(), plot.theme().seriesColor(0));

    // A pen's own dashes, which QPen measures in line widths, are kept in pixels.
    QPen dashed(QColor(Qt::blue), 2.0);
    dashed.setDashPattern({3.0, 2.0});
    line->setPen(dashed);
    EXPECT_EQ(line->dashPattern(), (QList<qreal>{6.0, 4.0}));
    EXPECT_EQ(line->pen().style(), Qt::CustomDashLine);
    EXPECT_EQ(line->pen().dashPattern(), (QList<qreal>{3.0, 2.0}));
    line->setLineWidth(4.0);  // as long in pixels
    EXPECT_EQ(line->pen().dashPattern(), (QList<qreal>{1.5, 1.0}));
    // A pen of a style takes the dashes away.
    line->setPen(QPen(QColor(Qt::blue), 2.0, Qt::DotLine));
    EXPECT_TRUE(line->dashPattern().isEmpty());
    EXPECT_EQ(line->pen().style(), Qt::DotLine);
}

TEST(LineSeries, SettersSignalOnlyChangesAndRefuseBadPatterns)
{
    PlotWidget       plot;
    LineSeries*      line = plot.addLine(std::vector<double>{1, 2});
    const QSignalSpy spy(line, &rocketplot::Series::changed);
    line->setDashPattern({});
    line->setMarkersAlwaysVisible(false);
    line->resetCapStyle();
    EXPECT_EQ(spy.count(), 0);
    for (const QList<qreal>& bad :
         {QList<qreal>{6.0}, QList<qreal>{6.0, 0.0}, QList<qreal>{6.0, -4.0}})
    {
        line->setDashPattern(bad);
    }
    EXPECT_EQ(spy.count(), 0);
    line->setDashPattern({6.0, 4.0});
    line->setCapStyle(Qt::FlatCap);
    line->setMarkersAlwaysVisible(true);
    line->setMarkerFilled(false);
    line->setMarkerRingWidth(0.0);
    EXPECT_EQ(spy.count(), 5);
    line->setMarkerRingWidth(-1.0);  // ignored
    EXPECT_DOUBLE_EQ(line->markerRingWidth(), 0.0);
    line->resetMarkerRingWidth();
    line->resetCapStyle();
    EXPECT_EQ(line->capStyle(), Qt::RoundCap);
    EXPECT_EQ(spy.count(), 7);
}

TEST_F(LineSeriesTest, MarkersCanShowHoweverClosePointsAre)
{
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i < 10'000; ++i)
    {
        x.push_back(i / 1000.0);
        y.push_back(5.0);
    }
    LineSeries* line = m_plot.addLine(x, y);
    line->setMarker(rocketplot::Marker::SQUARE);
    line->setMarkerSize(12.0);
    const QImage crowded = render();  // too close for markers
    line->setMarkersAlwaysVisible(true);
    const QImage marked = render();
    // Markers above and below the line, all along it.
    for (const double at : {0.5, 2.5, 5.0, 7.5, 9.5})
    {
        const QPoint above = pixelAt(at, 5.0, QPoint(0, -4));
        EXPECT_NE(crowded.pixelColor(above), line->color()) << at;
        EXPECT_EQ(marked.pixelColor(above), line->color()) << at;
    }
}

TEST_F(LineSeriesTest, DashesKeepTheirLengthsAlongTheWholeLine)
{
    // Many points: the line is drawn in pieces, and its pattern must go on across them.
    std::vector<double> x;
    std::vector<double> y;
    for (int i = 0; i <= 5000; ++i)
    {
        x.push_back(-1.0 + (i * 12.0 / 5000.0));
        y.push_back(5.0);
    }
    LineSeries* line = m_plot.addLine(x, y);
    line->setColor(Qt::red);
    line->setLineWidth(3.0);
    line->setCapStyle(Qt::FlatCap);
    line->setDashPattern({6.0, 4.0});
    const QImage           image = render();
    const QRectF           area  = m_plot.plotArea();
    std::vector<int>       gaps;
    const std::vector<int> dashes =
        runsAlong(image, pixelAt(5.0, 5.0).y(), static_cast<int>(area.left()) + 2,
                  static_cast<int>(area.right()) - 2, Qt::red, gaps);
    ASSERT_GT(dashes.size(), 50U);
    for (const int dash : dashes)
    {
        EXPECT_NEAR(dash, 6, 1);
    }
    for (const int gap : gaps)
    {
        EXPECT_NEAR(gap, 4, 1);
    }
}

TEST_F(LineSeriesTest, TheLegendSwatchIsDashedToo)
{
    LineSeries* dashed =
        m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{2, 2}, "dashed");
    dashed->setColor(Qt::red);
    dashed->setCapStyle(Qt::FlatCap);
    dashed->setDashPattern({4.0, 4.0});
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{8, 8}, "solid");
    const QImage     image  = render();
    const QRectF     legend = m_plot.legendArea();
    std::vector<int> gaps;
    // The first entry's swatch: 24 pixels from 8 pixels in, across the middle of its row.
    for (int y = static_cast<int>(legend.top()) + 8; y < static_cast<int>(legend.top()) + 30; ++y)
    {
        const std::vector<int> dashes =
            runsAlong(image, y, static_cast<int>(legend.left()) + 8,
                      static_cast<int>(legend.left()) + 32, Qt::red, gaps);
        if (!dashes.empty())
        {
            EXPECT_FALSE(gaps.empty());
            return;
        }
    }
    ADD_FAILURE() << "no dashes in the swatch";
}

}  // namespace
