#include "rocketplot/IconAnnotation.h"

#include <QColor>
#include <QIcon>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
#include <QSizeF>
#include <QTemporaryDir>
#include <Qt>
#include <algorithm>
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

using rocketplot::IconAnnotation;
using rocketplot::PlotWidget;

// A solid red square.
QIcon redIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::red);
    return {pixmap};
}

// An icon that is red at any size and remembers the scales (device pixel ratios) it was drawn at.
class RecordingEngine : public QIconEngine
{
public:
    explicit RecordingEngine(std::vector<double>* scales) : m_scales(scales) { }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode /*mode*/,
               QIcon::State /*state*/) override
    {
        painter->fillRect(rect, Qt::red);
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode /*mode*/, QIcon::State /*state*/,
                         qreal        scale) override
    {
        m_scales->push_back(scale);
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::red);
        return pixmap;
    }
    [[nodiscard]] QIconEngine* clone() const override
    {
        return new RecordingEngine(m_scales);  // NOLINT(cppcoreguidelines-owning-memory): Qt's API
    }

private:
    std::vector<double>* m_scales;
};

class IconAnnotationTest : public rocketplot::test::RenderedPlotTest
{
protected:
    // The smallest rectangle around the red pixels of @p image; empty if there are none.
    [[nodiscard]] static QRect redIn(const QImage& image)
    {
        QRect red;
        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                if (image.pixelColor(x, y) == QColor(Qt::red))
                {
                    red |= QRect(x, y, 1, 1);
                }
            }
        }
        return red;
    }

    [[nodiscard]] static QPointF centerOf(const QRect& rect)
    {
        return {rect.left() + (rect.width() / 2.0), rect.top() + (rect.height() / 2.0)};
    }
};

TEST(IconAnnotation, DefaultsAndSetters)
{
    PlotWidget      plot;
    IconAnnotation* icon = plot.addIcon(2.0, 3.0, redIcon());
    EXPECT_EQ(icon->position(), QPointF(2.0, 3.0));
    EXPECT_FALSE(icon->icon().isNull());
    EXPECT_EQ(icon->size(), QSizeF(16.0, 16.0));
    EXPECT_EQ(icon->alignment(), Qt::AlignCenter);
    EXPECT_EQ(icon->offset(), QPointF());
    EXPECT_EQ(icon->layer(), rocketplot::AnnotationLayer::ABOVE_SERIES);

    const QSignalSpy changed(icon, &rocketplot::Annotation::changed);
    icon->setPosition(2.0, 3.0);
    icon->setIcon(icon->icon());
    icon->resetSize();
    EXPECT_EQ(changed.count(), 0);
    icon->setPosition(4.0, 5.0);
    icon->setIcon(QIcon());
    icon->setSize(QSizeF(24.0, 12.0));
    icon->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    icon->setOffset(QPointF(3.0, 4.0));
    icon->setToolTip("Apogee");
    EXPECT_EQ(changed.count(), 6);
    icon->setSize(QSizeF(0.0, 12.0));  // ignored
    EXPECT_EQ(icon->size(), QSizeF(24.0, 12.0));
    icon->resetSize();
    EXPECT_EQ(icon->size(), QSizeF(16.0, 16.0));
}

TEST_F(IconAnnotationTest, IsCenteredOnItsPoint)
{
    m_plot.addIcon(5.0, 5.0, redIcon());
    const QRect red = redIn(render());
    EXPECT_EQ(red.size(), QSize(16, 16));
    const QPointF point = m_plot.mapFromData(QPointF(5.0, 5.0));
    EXPECT_NEAR(centerOf(red).x(), point.x(), 0.51);
    EXPECT_NEAR(centerOf(red).y(), point.y(), 0.51);
}

TEST_F(IconAnnotationTest, AlignmentOffsetAndSize)
{
    IconAnnotation* icon = m_plot.addIcon(5.0, 5.0, redIcon());
    icon->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);  // standing on the point
    icon->setOffset(QPointF(0.0, -2.0));
    icon->setSize(QSizeF(24.0, 24.0));
    const QRect   red   = redIn(render());
    const QPointF point = m_plot.mapFromData(QPointF(5.0, 5.0));
    EXPECT_EQ(red.size(), QSize(24, 24));
    EXPECT_NEAR(red.bottom() + 1, point.y() - 2.0, 0.51);
    EXPECT_NEAR(centerOf(red).x(), point.x(), 0.51);
}

TEST_F(IconAnnotationTest, NothingToDrawWithoutAnIconOrInView)
{
    const QImage    plain = render();
    IconAnnotation* icon  = m_plot.addIcon(5.0, 5.0, QIcon());
    EXPECT_EQ(render(), plain);
    icon->setIcon(redIcon());
    icon->setPosition(50.0, 5.0);
    EXPECT_TRUE(redIn(render()).isEmpty());
}

TEST_F(IconAnnotationTest, FollowsItsYAxisAndThePlotArea)
{
    m_plot.yAxis2()->setRange(0.0, 100.0);
    m_plot.addLine(std::vector<double>{0, 1}, std::vector<double>{0, 1})->setOnSecondaryYAxis(true);
    IconAnnotation* icon = m_plot.addIcon(5.0, 20.0, redIcon());
    icon->setOnSecondaryYAxis(true);
    const QPointF onRight = m_plot.mapFromData(QPointF(5.0, 20.0), m_plot.yAxis2());
    EXPECT_NEAR(centerOf(redIn(render())).y(), onRight.y(), 0.51);

    icon->setOnSecondaryYAxis(false);
    icon->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    icon->setPosition(1.0, 0.0);  // the bottom-right corner
    icon->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    const QRect red = redIn(render());
    EXPECT_EQ(red.right() + 1, static_cast<int>(m_plot.plotArea().right()));
    EXPECT_EQ(red.bottom() + 1, static_cast<int>(m_plot.plotArea().bottom()));
}

TEST_F(IconAnnotationTest, AutoscaleMakesRoomOnlyWhenAsked)
{
    m_plot.xAxis()->setAutoscale(true);
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 10});
    IconAnnotation* icon = m_plot.addIcon(40.0, 5.0, redIcon());
    EXPECT_LT(m_plot.xAxis()->max(), 20.0);
    icon->setIncludedInAutoscale(true);
    EXPECT_GT(m_plot.xAxis()->max(), 40.0);
}

TEST_F(IconAnnotationTest, SharpAtTheResolutionOfEveryExport)
{
    std::vector<double> scales;
    m_plot.addIcon(5.0, 5.0, QIcon(new RecordingEngine(&scales)));  // NOLINT: QIcon owns it

    static_cast<void>(m_plot.renderToImage({.dpi = 288.0}));
    ASSERT_FALSE(scales.empty());
    EXPECT_DOUBLE_EQ(scales.back(), 3.0);

    scales.clear();
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(m_plot.exportPdf(directory.filePath("icon.pdf")));
    ASSERT_FALSE(scales.empty());
    EXPECT_GE(*std::ranges::min_element(scales), 4.0);

    scales.clear();
    ASSERT_TRUE(m_plot.exportSvg(directory.filePath("icon.svg")));
    ASSERT_FALSE(scales.empty());
    EXPECT_GE(*std::ranges::min_element(scales), 4.0);
}

}  // namespace
