#include "rocketplot/TextAnnotation.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <Qt>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::TextAnnotation;

using TextAnnotationTest = rocketplot::test::RenderedPlotTest;

// A square of pixels around @p center.
QRect around(QPoint center, int reach)
{
    return {center.x() - reach, center.y() - reach, (2 * reach) + 1, (2 * reach) + 1};
}

TEST(TextAnnotation, Defaults)
{
    PlotWidget            plot;
    const TextAnnotation* note = plot.addText(2.0, 3.0, "note");
    EXPECT_EQ(note->position(), QPointF(2.0, 3.0));
    EXPECT_EQ(note->text(), "note");
    EXPECT_EQ(note->offset(), QPointF());
    EXPECT_EQ(note->alignment(), Qt::AlignCenter);
    EXPECT_TRUE(note->isArrowVisible());
    EXPECT_TRUE(note->isBackgroundVisible());
    EXPECT_FALSE(note->isFrameVisible());
    EXPECT_EQ(note->backgroundColor(), plot.theme().legendBackground);
    EXPECT_EQ(note->coordinates(), rocketplot::AnnotationCoordinates::DATA);
}

TEST(TextAnnotation, SettersSignalOnlyChanges)
{
    PlotWidget       plot;
    TextAnnotation*  note = plot.addText(2.0, 3.0, "note");
    const QSignalSpy changed(note, &rocketplot::Annotation::changed);
    note->setPosition(2.0, 3.0);
    note->setText("note");
    EXPECT_EQ(changed.count(), 0);
    note->setPosition(4.0, 5.0);
    note->setText("Max Q");
    note->setOffset(QPointF(30.0, -20.0));
    note->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    note->setArrowVisible(false);
    note->setBackgroundVisible(false);
    note->setFrameVisible(true);
    note->setBackgroundColor(Qt::yellow);
    note->setBackgroundColor(Qt::yellow);
    note->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    EXPECT_EQ(changed.count(), 9);
    EXPECT_EQ(note->position(), QPointF(4.0, 5.0));
}

TEST_F(TextAnnotationTest, IsDrawnAtItsPoint)
{
    const QImage plain = render();
    m_plot.addText(5.0, 5.0, "Apogee");
    const QImage image = render();
    EXPECT_TRUE(differ(image, plain, around(pixelAt(5.0, 5.0), 8)));
    EXPECT_FALSE(differ(image, plain, around(pixelAt(2.0, 2.0), 40)));
    // It moves with the data.
    m_plot.xAxis()->setRange(0.0, 20.0);
    const QImage plainWide = [&] {
        m_plot.annotations().front()->setVisible(false);
        const QImage without = render();
        m_plot.annotations().front()->setVisible(true);
        return without;
    }();
    EXPECT_TRUE(differ(render(), plainWide, around(pixelAt(5.0, 5.0), 8)));
    EXPECT_FALSE(differ(render(), plainWide, around(pixelAt(10.0, 5.0), 8)));
}

TEST_F(TextAnnotationTest, AlignmentSaysWhichPartIsAtThePoint)
{
    const QImage    plain = render();
    TextAnnotation* note  = m_plot.addText(5.0, 5.0, "Apogee");
    note->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    const QPoint point = pixelAt(5.0, 5.0);
    // Its bottom-left corner is at the point: the text is above and right of it.
    EXPECT_TRUE(differ(render(), plain, QRect(point.x() + 2, point.y() - 16, 30, 12)));
    EXPECT_FALSE(differ(render(), plain, QRect(point.x() - 40, point.y() - 20, 38, 60)));
    EXPECT_FALSE(differ(render(), plain, QRect(point.x() - 40, point.y() + 2, 120, 30)));
}

TEST_F(TextAnnotationTest, AnOffsetMovesItAwayWithAnArrowBack)
{
    const QImage    plain = render();
    TextAnnotation* note  = m_plot.addText(5.0, 5.0, "Apogee");
    note->setOffset(QPointF(120.0, -90.0));
    const QPoint point  = pixelAt(5.0, 5.0);
    const QPoint middle = point + QPoint(60, -45);
    EXPECT_TRUE(differ(render(), plain, around(point + QPoint(120, -90), 8)));  // the text
    EXPECT_TRUE(differ(render(), plain, around(middle, 3)));                    // the arrow
    EXPECT_TRUE(differ(render(), plain, around(point + QPoint(8, -6), 3)));     // its head
    EXPECT_FALSE(differ(render(), plain, around(point, 1)));  // which stops short of the point

    note->setArrowVisible(false);
    EXPECT_FALSE(differ(render(), plain, around(middle, 3)));
    EXPECT_TRUE(differ(render(), plain, around(point + QPoint(120, -90), 8)));
}

TEST_F(TextAnnotationTest, ItsBackgroundKeepsItReadableOverData)
{
    // A broad line, and text whose top-left corner is on it.
    auto* data = m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{5, 5});
    data->setLineWidth(20.0);
    TextAnnotation* note = m_plot.addText(5.0, 5.0, "Apogee");
    note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    note->setOffset(QPointF(0.0, -8.0));
    // In the margin the text's box leaves around the text, whatever the font: left of its first
    // letter and clear of the box's round corners.
    const QPoint margin = pixelAt(5.0, 5.0) + QPoint(2, -2);
    EXPECT_NE(render().pixelColor(margin), data->color());  // the box covers the line
    note->setBackgroundVisible(false);
    EXPECT_EQ(render().pixelColor(margin), data->color());
    note->setBackgroundVisible(true);
    note->setLayer(rocketplot::AnnotationLayer::BELOW_SERIES);
    EXPECT_EQ(render().pixelColor(margin), data->color());  // under the line now
}

TEST_F(TextAnnotationTest, NothingToDrawWithoutTextOrAPlace)
{
    const QImage    plain = render();
    TextAnnotation* note  = m_plot.addText(5.0, 5.0, QString());
    note->setOffset(QPointF(50.0, 50.0));
    EXPECT_EQ(render(), plain);
    note->setText("far away");
    note->setPosition(1e9, 5.0);
    EXPECT_EQ(render(), plain);
    note->setPosition(5.0, std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(render(), plain);
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.yAxis()->setRange(1.0, 100.0);
    const QImage plainLog = render();
    note->setPosition(5.0, -1.0);  // no place on a log axis
    EXPECT_EQ(render(), plainLog);
}

TEST_F(TextAnnotationTest, PinnedToThePlotAreaItStaysPutAsTheViewChanges)
{
    TextAnnotation* note = m_plot.addText(0.5, 0.5, "Pinned");
    note->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    note->setBackgroundVisible(false);
    const auto inkOf = [this, note] {
        const QImage with = render();
        note->setVisible(false);
        const QImage without = render();
        note->setVisible(true);
        return inkRect(with, without, with.rect());
    };
    const QRect ink = inkOf();
    ASSERT_FALSE(ink.isEmpty());
    EXPECT_NEAR(ink.center().x(), m_plot.plotArea().center().x(), 2.0);
    EXPECT_NEAR(ink.center().y(), m_plot.plotArea().center().y(), 3.0);
    // Pinned to the top-left corner, it stays there as the view changes (the corner itself may
    // move as the tick labels get wider or narrower).
    note->setPosition(0.0, 1.0);
    note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const auto inArea = [this](const QRect& rect) {
        return rect.translated(-m_plot.plotArea().topLeft().toPoint());
    };
    const QRect placed = inArea(inkOf());
    EXPECT_LT(placed.left(), 10);
    EXPECT_LT(placed.top(), 10);
    m_plot.xAxis()->setRange(100.0, 300.0);
    m_plot.yAxis()->setRange(-5.0, 5.0);
    EXPECT_EQ(inArea(inkOf()), placed);

    // (0, 0) is the bottom-left corner.
    note->setPosition(0.0, 0.0);
    note->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    const QRect corner = inkOf();
    const QRect area   = m_plot.plotArea().toAlignedRect();
    EXPECT_GE(corner.left(), area.left());
    EXPECT_LT(corner.left(), area.left() + 10);
    EXPECT_LE(corner.bottom(), area.bottom());
    EXPECT_GT(corner.bottom(), area.bottom() - 10);
}

TEST_F(TextAnnotationTest, PinnedItIsNotAutoscaled)
{
    m_plot.xAxis()->setAutoscale(true);
    m_plot.yAxis()->setAutoscale(true);
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 10});
    const rocketplot::Range x    = m_plot.xAxis()->range();
    TextAnnotation*         note = m_plot.addText(500.0, 500.0, "far");
    note->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    note->setIncludedInAutoscale(true);
    EXPECT_EQ(m_plot.xAxis()->range(), x);
    note->setCoordinates(rocketplot::AnnotationCoordinates::DATA);
    EXPECT_GT(m_plot.xAxis()->max(), 400.0);
}

TEST_F(TextAnnotationTest, AFrameAndABackgroundColor)
{
    TextAnnotation* note = m_plot.addText(5.0, 5.0, "Summary");
    note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const QImage plain = render();
    // The box's left edge, half way down, and a pixel inside it.
    const QPoint corner = pixelAt(5.0, 5.0);
    note->setFrameVisible(true);
    const QImage framed = render();
    EXPECT_TRUE(differ(framed, plain, QRect(corner.x() - 1, corner.y() + 3, 3, 6)));
    // A box of another color: where the text isn't, it is that color.
    note->setFrameVisible(false);
    note->setBackgroundColor(QColor(255, 255, 0));
    const QPoint inside = corner + QPoint(2, 2);
    EXPECT_EQ(render().pixelColor(inside), QColor(255, 255, 0));
    note->resetBackgroundColor();
    EXPECT_EQ(render(), plain);
}

TEST_F(TextAnnotationTest, RichText)
{
    const QImage plain = render();
    m_plot.addText(5.0, 5.0, "T<sub>max</sub> = <b>312 K</b>")->setOffset(QPointF(-60.0, 40.0));
    EXPECT_TRUE(differ(render(), plain, around(pixelAt(5.0, 5.0) + QPoint(-60, 40), 10)));
}

}  // namespace
