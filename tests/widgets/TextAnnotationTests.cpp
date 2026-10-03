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
    EXPECT_EQ(changed.count(), 6);
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
    auto* data = m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{5, 5});
    data->setLineWidth(4.0);
    TextAnnotation* note = m_plot.addText(5.0, 5.0, "Apogee");
    // The line's pixels along the row through the middle of the text.
    const auto lineShows = [&] {
        const QImage image = render();
        const QPoint point = pixelAt(5.0, 5.0);
        int          count = 0;
        for (int x = point.x() - 15; x <= point.x() + 15; ++x)
        {
            count += image.pixelColor(x, point.y()) == data->color() ? 1 : 0;
        }
        return count;
    };
    EXPECT_EQ(lineShows(), 0);
    note->setBackgroundVisible(false);
    EXPECT_GT(lineShows(), 5);
    note->setLayer(rocketplot::AnnotationLayer::BELOW_SERIES);
    EXPECT_EQ(lineShows(), 31);  // under the line now
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

TEST_F(TextAnnotationTest, RichText)
{
    const QImage plain = render();
    m_plot.addText(5.0, 5.0, "T<sub>max</sub> = <b>312 K</b>")->setOffset(QPointF(-60.0, 40.0));
    EXPECT_TRUE(differ(render(), plain, around(pixelAt(5.0, 5.0) + QPoint(-60, 40), 10)));
}

}  // namespace
