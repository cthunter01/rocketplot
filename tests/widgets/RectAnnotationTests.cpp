#include "rocketplot/RectAnnotation.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <Qt>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Annotation.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::PlotWidget;
using rocketplot::RectAnnotation;

class RectAnnotationTest : public rocketplot::test::RenderedPlotTest
{
protected:
    // A rectangle from (2, 2) to (6, 8), outlined 3 pixels wide in red.
    RectAnnotation* addRed()
    {
        RectAnnotation* rect = m_plot.addRect(QRectF(QPointF(2.0, 2.0), QPointF(6.0, 8.0)));
        rect->setColor(Qt::red);
        rect->setLineWidth(3.0);
        return rect;
    }

    [[nodiscard]] static bool isRed(const QColor& color) { return color == QColor(Qt::red); }
};

TEST(RectAnnotation, Defaults)
{
    PlotWidget            plot;
    const RectAnnotation* rect = plot.addRect(QRectF(QPointF(6.0, 8.0), QPointF(2.0, 2.0)), "box");
    EXPECT_EQ(rect->rect(), QRectF(QPointF(2.0, 2.0), QPointF(6.0, 8.0)));  // corners in order
    EXPECT_DOUBLE_EQ(rect->xRange().min, 2.0);
    EXPECT_DOUBLE_EQ(rect->yRange().max, 8.0);
    EXPECT_EQ(rect->label(), "box");
    EXPECT_DOUBLE_EQ(rect->lineWidth(), plot.theme().annotationLineWidth);
    EXPECT_EQ(rect->lineStyle(), Qt::SolidLine);
    EXPECT_DOUBLE_EQ(rect->fillOpacity(), 0.0);
    EXPECT_EQ(rect->labelAlignment(), Qt::AlignLeft | Qt::AlignTop);
    EXPECT_EQ(rect->layer(), rocketplot::AnnotationLayer::BELOW_SERIES);
    EXPECT_EQ(rect->color(), plot.theme().annotation);
    EXPECT_EQ(plot.annotations().size(), 1);
}

TEST(RectAnnotation, SettersSignalOnlyChanges)
{
    PlotWidget       plot;
    RectAnnotation*  rect = plot.addRect(QRectF(0.0, 0.0, 1.0, 1.0));
    const QSignalSpy changed(rect, &rocketplot::Annotation::changed);
    rect->setRect(QRectF(QPointF(1.0, 1.0), QPointF(0.0, 0.0)));  // the same, the other way round
    rect->setFillOpacity(0.0);
    rect->resetLineWidth();
    EXPECT_EQ(changed.count(), 0);
    rect->setRect(QRectF(0.0, 0.0, 2.0, 2.0));
    rect->setLabel("box");
    rect->setLineWidth(2.0);
    rect->setLineStyle(Qt::DashLine);
    rect->setFillOpacity(2.0);  // clamped
    rect->setLabelAlignment(Qt::AlignCenter);
    EXPECT_EQ(changed.count(), 6);
    EXPECT_DOUBLE_EQ(rect->fillOpacity(), 1.0);
    rect->setLineWidth(-1.0);  // ignored
    rect->setFillOpacity(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(changed.count(), 6);
    rect->resetFillOpacity();
    rect->resetLineWidth();
    EXPECT_EQ(changed.count(), 8);
}

TEST_F(RectAnnotationTest, IsOutlinedAndNotFilled)
{
    const QImage plain = render();
    addRed();
    const QImage image = render();
    for (const QPointF& edge : {QPointF(2.0, 5.0), QPointF(6.0, 5.0), QPointF(4.0, 2.0),
                                QPointF(4.0, 8.0), QPointF(2.0, 2.0), QPointF(6.0, 8.0)})
    {
        EXPECT_TRUE(isRed(image.pixelColor(pixelAt(edge.x(), edge.y())))) << edge.x() << edge.y();
    }
    EXPECT_EQ(image.pixelColor(pixelAt(4.0, 5.0)), plain.pixelColor(pixelAt(4.0, 5.0)));
    EXPECT_EQ(image.pixelColor(pixelAt(1.0, 5.0)), plain.pixelColor(pixelAt(1.0, 5.0)));
}

TEST_F(RectAnnotationTest, AFillAndNoOutline)
{
    const QImage    plain = render();
    RectAnnotation* rect  = addRed();
    rect->setFillOpacity(1.0);
    EXPECT_TRUE(isRed(render().pixelColor(pixelAt(4.0, 5.0))));
    rect->setFillOpacity(0.0);
    rect->setLineStyle(Qt::NoPen);
    EXPECT_EQ(render(), plain);
}

TEST_F(RectAnnotationTest, ItsLabelIsInside)
{
    RectAnnotation* rect  = addRed();
    const QImage    plain = render();
    rect->setLabel("Search domain");
    const QImage image = render();
    const QRect  inside(pixelAt(2.0, 8.0), pixelAt(6.0, 2.0));
    // At the top left, inside the outline.
    const QRect ink = inkRect(plain, image, image.rect());
    ASSERT_FALSE(ink.isEmpty());
    EXPECT_TRUE(inside.contains(ink));
    EXPECT_LT(ink.top() - inside.top(), 20);
    EXPECT_LT(ink.left() - inside.left(), 20);
}

TEST_F(RectAnnotationTest, ANaNEdgeDrawsNothing)
{
    const QImage    plain = render();
    RectAnnotation* rect  = addRed();
    rect->setRect(
        QRectF(QPointF(2.0, std::numeric_limits<double>::quiet_NaN()), QPointF(6.0, 8.0)));
    EXPECT_EQ(render(), plain);
    rect->setRect(QRectF(QPointF(20.0, 2.0), QPointF(30.0, 8.0)));  // out of view
    EXPECT_EQ(render(), plain);
}

TEST_F(RectAnnotationTest, IsCutOffAtThePlotArea)
{
    // From left of the plot to its middle: no outline along the plot's left edge, the top and
    // bottom edges run to it.
    RectAnnotation* rect = addRed();
    rect->setRect(QRectF(QPointF(-5.0, 2.0), QPointF(6.0, 8.0)));
    const QImage image = render();
    EXPECT_TRUE(isRed(image.pixelColor(pixelAt(0.0, 2.0, QPoint(2, 0)))));
    EXPECT_FALSE(isRed(image.pixelColor(pixelAt(0.0, 5.0, QPoint(1, 0)))));
    EXPECT_FALSE(isRed(image.pixelColor(pixelAt(0.0, 5.0, QPoint(-3, 0)))));
}

TEST_F(RectAnnotationTest, AutoscaleOnTheSecondaryAxis)
{
    m_plot.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1})->setOnSecondaryYAxis(true);
    m_plot.yAxis2()->setAutoscale(true);
    RectAnnotation* rect = addRed();
    rect->setOnSecondaryYAxis(true);
    rect->setIncludedInAutoscale(true);
    EXPECT_GE(m_plot.yAxis2()->max(), 8.0);
    // Drawn on that axis.
    const QPointF edge = m_plot.mapFromData(QPointF(2.0, 5.0), m_plot.yAxis2());
    EXPECT_TRUE(isRed(render().pixelColor(edge.toPoint())));
}

TEST_F(RectAnnotationTest, PinnedToThePlotArea)
{
    RectAnnotation* rect = addRed();
    rect->setRect(QRectF(QPointF(0.25, 0.25), QPointF(0.75, 0.75)));
    rect->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    m_plot.yAxis()->setRange(-100.0, 100.0);
    const QRectF area  = m_plot.plotArea();
    const QImage image = render();
    EXPECT_TRUE(isRed(image.pixelColor(
        QPointF(area.left() + (0.25 * area.width()), area.center().y()).toPoint())));
    EXPECT_TRUE(isRed(image.pixelColor(
        QPointF(area.center().x(), area.bottom() - (0.75 * area.height())).toPoint())));
}

}  // namespace
