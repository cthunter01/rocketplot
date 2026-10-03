#include "rocketplot/EventMarker.h"

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <Qt>
#include <algorithm>
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

using rocketplot::EventMarker;
using rocketplot::PlotWidget;

constexpr int kNoFlag = -1;

// Events are drawn in plain colors here, so their lines and flags can be found by color.
class EventMarkerTest : public rocketplot::test::RenderedPlotTest
{
protected:
    EventMarker* addEvent(double x, const QString& label, const QColor& color)
    {
        EventMarker* event = m_plot.addEvent(x, label);
        event->setColor(color);
        return event;
    }

    // The pixel column an event's line is drawn in.
    [[nodiscard]] int columnOf(double x) const
    {
        return static_cast<int>(std::floor(m_plot.mapFromData(QPointF(x, 0.0)).x()));
    }

    // The first row from the top of the plot where column @p x has @p color: where a flag of that
    // color starts. kNoFlag if there is none.
    [[nodiscard]] int flagTop(const QImage& image, int x, const QColor& color) const
    {
        const QRect plot = m_plot.plotArea().toRect();
        for (int y = plot.top(); y < plot.center().y(); ++y)
        {
            if (image.pixelColor(x, y) == color)
            {
                return y;
            }
        }
        return kNoFlag;
    }

    // The row where the flag of the event at @p x starts, whichever side of its line it flies on (a
    // wide label near the right edge flies to the left, and how wide a label is depends on the
    // platform's fonts). kNoFlag if there is none.
    [[nodiscard]] int flagRow(const QImage& image, double x, const QColor& color) const
    {
        const int right = flagTop(image, columnOf(x) + 3, color);
        return right != kNoFlag ? right : flagTop(image, columnOf(x) - 3, color);
    }
};

TEST(EventMarker, Defaults)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    EventMarker* event = plot.addEvent(150.0, "MECO");
    EXPECT_DOUBLE_EQ(event->x(), 150.0);
    EXPECT_EQ(event->label(), "MECO");
    EXPECT_EQ(event->color(), plot.theme().annotation);

    const QSignalSpy changed(event, &rocketplot::Annotation::changed);
    event->setX(150.0);
    event->setLabel("MECO");
    EXPECT_EQ(changed.count(), 0);
    event->setX(151.0);
    event->setLabel("Main engine cutoff");
    EXPECT_EQ(changed.count(), 2);
}

TEST_F(EventMarkerTest, IsALineWithAFlagAtTheTop)
{
    const QImage plain = render();
    addEvent(4.0, "MECO", Qt::magenta);
    const QImage image  = render();
    const int    column = columnOf(4.0);
    const QRect  plot   = m_plot.plotArea().toRect();
    // The line, from the top of the plot to the bottom.
    for (const int y : {plot.top() + 1, plot.center().y(), plot.bottom() - 1})
    {
        EXPECT_EQ(image.pixelColor(column, y), QColor(Qt::magenta)) << y;
    }
    EXPECT_EQ(image.pixelColor(column + 3, plot.center().y()),
              plain.pixelColor(column + 3, plot.center().y()));
    // The flag, to the right of the line at the top.
    const int top = flagTop(image, column + 3, Qt::magenta);
    EXPECT_NE(top, kNoFlag);
    EXPECT_LT(top, plot.top() + 6);
    EXPECT_EQ(flagTop(image, column - 3, Qt::magenta), kNoFlag);
}

TEST_F(EventMarkerTest, WithoutALabelThereIsOnlyTheLine)
{
    addEvent(4.0, QString(), Qt::magenta);
    const QImage image = render();
    EXPECT_EQ(image.pixelColor(columnOf(4.0), m_plot.plotArea().toRect().center().y()),
              QColor(Qt::magenta));
    EXPECT_EQ(flagTop(image, columnOf(4.0) + 3, Qt::magenta), kNoFlag);
}

TEST_F(EventMarkerTest, FlagsTooCloseToShareARowAreStaggered)
{
    // A few pixels apart: in the same row, the second flag would cover the first.
    addEvent(4.0, "MECO", Qt::cyan);
    EventMarker* second = addEvent(4.05, "SEP", Qt::magenta);
    const QImage both   = render();
    const int    first  = flagRow(both, 4.0, Qt::cyan);
    ASSERT_NE(first, kNoFlag);
    EXPECT_GT(flagRow(both, 4.05, Qt::magenta), first + 8);  // a row further down

    // Far enough apart, they share the top row.
    second->setX(7.0);
    EXPECT_EQ(flagRow(render(), 7.0, Qt::magenta), first);
    // As they do once zoomed in.
    second->setX(4.05);
    m_plot.xAxis()->setRange(3.99, 4.09);
    const QImage zoomed = render();
    EXPECT_EQ(flagRow(zoomed, 4.0, Qt::cyan), first);
    EXPECT_EQ(flagRow(zoomed, 4.05, Qt::magenta), first);
}

TEST_F(EventMarkerTest, AFlagAtTheRightEdgeFliesToTheLeft)
{
    addEvent(9.9, "Landing", Qt::magenta);
    const QImage image = render();
    EXPECT_NE(flagTop(image, columnOf(9.9) - 3, Qt::magenta), kNoFlag);
    EXPECT_EQ(flagTop(image, columnOf(9.9) + 3, Qt::magenta), kNoFlag);
}

TEST_F(EventMarkerTest, FlagsWithoutRoomAreLeftOutButTheirLinesStay)
{
    // 300 events a pixel or two apart: far more flags than the rows that fit.
    for (int i = 0; i < 300; ++i)
    {
        addEvent(2.0 + (i * 0.02), QStringLiteral("Event %1").arg(i), Qt::magenta);
    }
    const QImage image = render();
    const QRect  plot  = m_plot.plotArea().toRect();
    // Below the rows of flags, only the lines.
    const int y = plot.bottom() - 20;
    EXPECT_EQ(image.pixelColor(columnOf(2.0), y), QColor(Qt::magenta));
    EXPECT_EQ(image.pixelColor(columnOf(8.0 - 0.02), y), QColor(Qt::magenta));
    int flagged = 0;
    for (int x = plot.left(); x <= plot.right(); ++x)
    {
        flagged += image.pixelColor(x, plot.bottom() - 40) == QColor(Qt::magenta) ? 1 : 0;
    }
    EXPECT_LT(flagged, 310);  // lines, not a wall of flags
}

TEST_F(EventMarkerTest, AnEventTheAxisCannotPlaceIsNotDrawn)
{
    const QImage plain = render();
    EventMarker* event = addEvent(50.0, "later", Qt::magenta);
    EXPECT_EQ(render(), plain);
    event->setX(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(render(), plain);
}

TEST_F(EventMarkerTest, TheFlagTextStandsOutOnItsColor)
{
    // A dark flag gets light text, a light one dark text.
    addEvent(2.0, "WW", QColor(0x20, 0x20, 0x20));
    addEvent(6.0, "WW", QColor(0xff, 0xf0, 0xa0));
    const QImage image    = render();
    const QRect  plot     = m_plot.plotArea().toRect();
    const auto   lightest = [&](int left) {
        float lightness = 0.0F;
        for (int y = plot.top(); y < plot.top() + 24; ++y)
        {
            for (int x = left + 5; x < left + 20; ++x)
            {
                lightness = std::max(lightness, image.pixelColor(x, y).lightnessF());
            }
        }
        return lightness;
    };
    const auto darkest = [&](int left) {
        float lightness = 1.0F;
        for (int y = plot.top() + 4; y < plot.top() + 16; ++y)
        {
            for (int x = left + 5; x < left + 20; ++x)
            {
                lightness = std::min(lightness, image.pixelColor(x, y).lightnessF());
            }
        }
        return lightness;
    };
    EXPECT_GT(lightest(columnOf(2.0)), 0.6F);
    EXPECT_LT(darkest(columnOf(6.0)), 0.4F);
}

}  // namespace
