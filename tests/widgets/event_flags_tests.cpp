// Where event flags go: over the data, with headroom above it, or in a band above the plot area.

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSignalSpy>
#include <QSize>
#include <Qt>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::EventMarker;
using rocketplot::FlagPlacement;
using rocketplot::PlotWidget;

// A red bump peaking at x = 4, and three blue events just before it whose flags, staggered into
// three rows, fly over its top.
class EventFlagsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        std::vector<double> x;
        std::vector<double> y;
        for (int i = 0; i <= 200; ++i)
        {
            x.push_back(i / 20.0);
            y.push_back(10.0 * std::exp(-std::pow(x.back() - 4.0, 2.0) / 2.0));
        }
        m_data = m_plot.addLine(x, y);
        m_data->setColor(Qt::red);
        for (const auto& [at, label] :
             {std::pair(3.0, "Ignition"), std::pair(3.3, "Max Q"), std::pair(3.6, "Burnout")})
        {
            EventMarker* event = m_plot.addEvent(at, label);
            event->setColor(Qt::blue);
            m_events.push_back(event);
        }
    }

    // Whether a point of the data is drawn under a flag, in the plot as it is now drawn.
    [[nodiscard]] bool dataUnderFlags()
    {
        static_cast<void>(m_plot.grab());
        for (std::size_t i = 0; i < m_data->size(); ++i)
        {
            const QPointF point = m_plot.mapFromData(QPointF(m_data->x(i), m_data->y(i)));
            if (qobject_cast<EventMarker*>(m_plot.annotationAt(point)) != nullptr)
            {
                return true;
            }
        }
        return false;
    }

    // Whether the red data stays clear of the blue flags, which are drawn over it, in the columns
    // from @p fromColumn to @p toColumn of @p image: in each, the data is drawn (not hidden under
    // a flag) and below the flags. Columns of an event's line, blue from top to bottom, are left
    // out.
    [[nodiscard]] static bool dataClearOfFlags(const QImage& image, int fromColumn, int toColumn)
    {
        for (int x = fromColumn; x < toColumn; ++x)
        {
            if (image.pixelColor(x, image.height() * 2 / 3) == QColor(Qt::blue))
            {
                continue;  // an event's line
            }
            int lowestBlue = -1;
            int highestRed = -1;
            for (int y = 0; y < image.height(); ++y)
            {
                const QColor color = image.pixelColor(x, y);
                if (color == QColor(Qt::blue))
                {
                    lowestBlue = y;
                }
                // Nearly red: some columns of the antialiased line have no pixel of pure red.
                const bool red = color.red() > 240 && color.green() < 32 && color.blue() < 32;
                if (red && highestRed < 0)
                {
                    highestRed = y;
                }
            }
            if (highestRed < 0 || (lowestBlue >= 0 && highestRed <= lowestBlue + 2))
            {
                return false;
            }
        }
        return true;
    }

    PlotWidget                m_plot;
    rocketplot::LineSeries*   m_data = nullptr;
    std::vector<EventMarker*> m_events;
};

TEST_F(EventFlagsTest, OverTheDataHorizontallyByDefault)
{
    EXPECT_EQ(m_plot.eventFlagPlacement(), FlagPlacement::OVER_DATA);
    EXPECT_EQ(m_plot.eventFlagOrientation(), Qt::Horizontal);
    const QSignalSpy placement(&m_plot, &PlotWidget::eventFlagPlacementChanged);
    const QSignalSpy orientation(&m_plot, &PlotWidget::eventFlagOrientationChanged);
    m_plot.setEventFlagPlacement(FlagPlacement::OVER_DATA);
    m_plot.setEventFlagOrientation(Qt::Horizontal);
    m_plot.setEventFlagPlacement(FlagPlacement::HEADROOM);
    m_plot.setEventFlagOrientation(Qt::Vertical);
    EXPECT_EQ(placement.count(), 1);
    EXPECT_EQ(orientation.count(), 1);
    // Over the data, the flags cover the top of the bump.
    m_plot.setEventFlagPlacement(FlagPlacement::OVER_DATA);
    m_plot.setEventFlagOrientation(Qt::Horizontal);
    EXPECT_TRUE(dataUnderFlags());
}

TEST_F(EventFlagsTest, WithoutFlagsEveryPlacementLaysOutTheSame)
{
    for (EventMarker* event : m_events)
    {
        event->setLabel({});  // only lines
    }
    const QImage over = m_plot.grab().toImage();
    const QRectF area = m_plot.plotArea();
    for (const FlagPlacement placement : {FlagPlacement::HEADROOM, FlagPlacement::ABOVE_PLOT_AREA})
    {
        m_plot.setEventFlagPlacement(placement);
        EXPECT_EQ(m_plot.grab().toImage(), over) << static_cast<int>(placement);
        EXPECT_EQ(m_plot.plotArea(), area);
    }
}

TEST_F(EventFlagsTest, HeadroomKeepsTheDataClearOfTheFlagsAtAnyHeight)
{
    m_plot.setEventFlagPlacement(FlagPlacement::HEADROOM);
    for (const int height : {160, 240, 400, 700, 1000})
    {
        m_plot.resize(800, height);
        m_plot.resetView();
        EXPECT_FALSE(dataUnderFlags()) << height;
    }
}

TEST_F(EventFlagsTest, HeadroomOnALogAxis)
{
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.setEventFlagPlacement(FlagPlacement::HEADROOM);
    EXPECT_FALSE(dataUnderFlags());
}

TEST_F(EventFlagsTest, HeadroomIsSettledAndOnlyWhileAutoscaling)
{
    m_plot.setEventFlagPlacement(FlagPlacement::HEADROOM);
    static_cast<void>(m_plot.grab());
    const rocketplot::Range settled = m_plot.yAxis()->range();
    EXPECT_GT(settled.max, 11.0);  // more room than the margin alone leaves above 10
    const QSignalSpy viewChanged(&m_plot, &PlotWidget::viewChanged);
    static_cast<void>(m_plot.grab());
    static_cast<void>(m_plot.grab());
    EXPECT_EQ(viewChanged.count(), 0);
    EXPECT_EQ(m_plot.yAxis()->range(), settled);
    // A range the user chose stays.
    m_plot.yAxis()->setRange(0.0, 10.0);
    static_cast<void>(m_plot.grab());
    EXPECT_EQ(m_plot.yAxis()->range(), (rocketplot::Range{.min = 0.0, .max = 10.0}));
    // Fewer rows of flags, less room.
    m_plot.resetView();
    m_events.back()->setLabel({});
    m_events[1]->setLabel({});
    static_cast<void>(m_plot.grab());
    EXPECT_LT(m_plot.yAxis()->max(), settled.max);
}

TEST_F(EventFlagsTest, AnExportOfAnotherHeightKeepsTheHeadroom)
{
    m_plot.setEventFlagPlacement(FlagPlacement::HEADROOM);
    m_plot.resize(800, 600);
    static_cast<void>(m_plot.grab());
    const auto column = [this](double x) {
        return static_cast<int>(m_plot.mapFromData(QPointF(x, 0.0)).x());
    };
    // Left and right of the plot the same in a shorter image: the flags right of the first line.
    EXPECT_TRUE(dataClearOfFlags(m_plot.grab().toImage(), column(3.0) + 2, column(3.0) + 40));
    const QImage shorter = m_plot.renderToImage({.size = QSize(800, 220), .dpi = 96.0});
    EXPECT_TRUE(dataClearOfFlags(shorter, column(3.0) + 2, column(3.0) + 40));
    // Without headroom the shorter one has data in the flags.
    m_plot.setEventFlagPlacement(FlagPlacement::OVER_DATA);
    EXPECT_FALSE(dataClearOfFlags(m_plot.renderToImage({.size = QSize(800, 220), .dpi = 96.0}),
                                  column(3.0) + 2, column(3.0) + 40));
}

TEST_F(EventFlagsTest, AboveThePlotAreaNoFlagIsInIt)
{
    m_plot.setTitle("Flight 7");
    const double plainTop = m_plot.plotArea().top();
    m_plot.setEventFlagPlacement(FlagPlacement::ABOVE_PLOT_AREA);
    static_cast<void>(m_plot.grab());
    const QRectF area = m_plot.plotArea();
    EXPECT_GT(area.top(), plainTop + 40.0);  // three rows of flags above it
    for (int y = static_cast<int>(area.top()) + 1; y < static_cast<int>(area.bottom()); y += 4)
    {
        for (int x = static_cast<int>(area.left()) + 1; x < static_cast<int>(area.right()); x += 4)
        {
            ASSERT_EQ(qobject_cast<EventMarker*>(m_plot.annotationAt(QPointF(x, y))), nullptr)
                << x << ", " << y;
        }
    }
    // They are above it, and their lines reach up to them.
    const double line = m_plot.mapFromData(QPointF(3.0, 0.0)).x();
    EXPECT_EQ(m_plot.annotationAt(QPointF(line + 6.0, plainTop + 6.0)), m_events.front());
    const QImage image = m_plot.grab().toImage();
    EXPECT_EQ(image.pixelColor(static_cast<int>(line), static_cast<int>(area.top()) - 2),
              QColor(Qt::blue));
}

TEST_F(EventFlagsTest, TheBandHoldsWhilePanning)
{
    m_plot.setEventFlagPlacement(FlagPlacement::ABOVE_PLOT_AREA);
    const double top = m_plot.plotArea().top();
    for (const double start : {-2.0, 1.0, 2.5})
    {
        m_plot.xAxis()->setRange(start, start + 10.0);
        EXPECT_DOUBLE_EQ(m_plot.plotArea().top(), top) << start;
    }
}

TEST_F(EventFlagsTest, VerticalFlagsReadUpwardInOneRow)
{
    m_plot.setEventFlagOrientation(Qt::Vertical);
    static_cast<void>(m_plot.grab());
    const QRectF area = m_plot.plotArea();
    for (const double at : {3.0, 3.3, 3.6})
    {
        const double line = m_plot.mapFromData(QPointF(at, 0.0)).x();
        // Each flag is narrow and tall, starting at the top.
        EXPECT_NE(m_plot.annotationAt(QPointF(line + 5.0, area.top() + 4.0)), nullptr) << at;
        EXPECT_NE(m_plot.annotationAt(QPointF(line + 5.0, area.top() + 30.0)), nullptr) << at;
        EXPECT_EQ(m_plot.annotationAt(QPointF(line + 40.0, area.top() + 30.0)), nullptr) << at;
    }
}

}  // namespace
