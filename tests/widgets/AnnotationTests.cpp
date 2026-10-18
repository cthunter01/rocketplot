#include "rocketplot/Annotation.h"

#include <QApplication>
#include <QColor>
#include <QHelpEvent>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QToolTip>
#include <QVariant>
#include <Qt>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
#include "rocketplot/IconAnnotation.h"
#include "rocketplot/Legend.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ReferenceLine.h"
#include "rocketplot/ShadedSpan.h"
#include "rocketplot/TextAnnotation.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::Annotation;
using rocketplot::AnnotationLayer;
using rocketplot::PlotWidget;

using AnnotationTest = rocketplot::test::RenderedPlotTest;

// A solid red square.
QIcon redIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::red);
    return {pixmap};
}

// A plot on screen, for the pointer and tool tips.
class PointingTest : public rocketplot::test::RenderedPlotTest
{
protected:
    void SetUp() override
    {
        RenderedPlotTest::SetUp();
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    }
    void TearDown() override { QToolTip::hideText(); }
};

TEST(Annotation, AddedAndRemoved)
{
    PlotWidget       plot;
    const QSignalSpy added(&plot, &PlotWidget::annotationAdded);
    const QSignalSpy removed(&plot, &PlotWidget::annotationRemoved);
    auto*            line  = plot.addHorizontalLine(1.0, "limit");
    auto*            span  = plot.addVerticalSpan(1.0, 2.0);
    auto*            text  = plot.addText(1.0, 1.0, "note");
    auto*            event = plot.addEvent(3.0, "event");
    EXPECT_EQ(plot.annotations(), (QList<Annotation*>{line, span, text, event}));
    EXPECT_EQ(added.count(), 4);
    EXPECT_EQ(line->plot(), &plot);
    EXPECT_EQ(line->label(), "limit");

    const QPointer<Annotation> watch(span);
    plot.removeAnnotation(span);
    EXPECT_TRUE(watch.isNull());
    EXPECT_EQ(removed.count(), 1);
    EXPECT_EQ(plot.annotations().size(), 3);
    plot.removeAnnotation(nullptr);  // not one of its own: ignored
    EXPECT_EQ(removed.count(), 1);
    plot.clearAnnotations();
    EXPECT_TRUE(plot.annotations().isEmpty());
    EXPECT_EQ(removed.count(), 4);
}

TEST(Annotation, ColorFollowsTheThemeUnlessSet)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    auto*       line = plot.addVerticalLine(1.0);
    const auto* text = plot.addText(1.0, 1.0, "note");
    EXPECT_EQ(line->color(), rocketplot::Theme::light().annotation);
    EXPECT_EQ(text->color(), rocketplot::Theme::light().text);  // text is in the text color
    plot.setThemeMode(rocketplot::ThemeMode::DARK);
    EXPECT_EQ(line->color(), rocketplot::Theme::dark().annotation);

    const QSignalSpy changed(line, &Annotation::changed);
    line->setColor(Qt::red);
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    EXPECT_EQ(line->color(), QColor(Qt::red));  // a color set by the user stays
    line->resetColor();
    EXPECT_EQ(line->color(), rocketplot::Theme::light().annotation);
    EXPECT_EQ(changed.count(), 2);
}

TEST(Annotation, LayersAndAxes)
{
    PlotWidget plot;
    auto*      span = plot.addHorizontalSpan(1.0, 2.0);
    EXPECT_EQ(span->layer(), AnnotationLayer::BELOW_SERIES);
    EXPECT_EQ(plot.addHorizontalLine(1.0)->layer(), AnnotationLayer::ABOVE_SERIES);
    EXPECT_EQ(plot.addText(1.0, 1.0, "note")->layer(), AnnotationLayer::ABOVE_SERIES);
    EXPECT_EQ(plot.addEvent(1.0, "event")->layer(), AnnotationLayer::ABOVE_SERIES);

    EXPECT_EQ(span->yAxis(), plot.yAxis());
    span->setYAxis(plot.yAxis2());
    EXPECT_TRUE(span->isOnSecondaryYAxis());
    span->setYAxis(plot.xAxis());  // not a y axis: ignored
    EXPECT_EQ(span->yAxis(), plot.yAxis2());

    EXPECT_TRUE(span->setProperty("layer", QVariant::fromValue(AnnotationLayer::ABOVE_SERIES)));
    EXPECT_EQ(span->layer(), AnnotationLayer::ABOVE_SERIES);
    EXPECT_TRUE(span->setProperty("visible", false));
    EXPECT_FALSE(span->isVisible());
}

TEST(Annotation, AutoscaleIgnoresThemUnlessIncluded)
{
    PlotWidget plot;
    plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 10});
    auto* limit = plot.addHorizontalLine(100.0);
    auto* event = plot.addEvent(-50.0, "before");
    auto* span  = plot.addVerticalSpan(20.0, 30.0);
    EXPECT_LT(plot.yAxis()->max(), 11.0);
    EXPECT_GT(plot.xAxis()->min(), -1.0);

    limit->setIncludedInAutoscale(true);
    EXPECT_GT(plot.yAxis()->max(), 100.0);
    event->setIncludedInAutoscale(true);
    span->setIncludedInAutoscale(true);
    EXPECT_LT(plot.xAxis()->min(), -50.0);
    EXPECT_GT(plot.xAxis()->max(), 30.0);
    limit->setValue(200.0);  // it moves: the axis follows
    EXPECT_GT(plot.yAxis()->max(), 200.0);

    // A hidden one makes no claim, nor does one that is removed.
    limit->setVisible(false);
    EXPECT_LT(plot.yAxis()->max(), 11.0);
    plot.removeAnnotation(span);
    EXPECT_LT(plot.xAxis()->max(), 15.0);
    EXPECT_LT(plot.xAxis()->min(), -50.0);  // the event still counts
}

TEST(Annotation, AutoscaleOnASecondaryOrLogAxis)
{
    PlotWidget plot;
    plot.addLine(std::vector<double>{1, 10}, std::vector<double>{1, 10});
    plot.addLine(std::vector<double>{1, 10}, std::vector<double>{100, 200})
        ->setYAxis(plot.yAxis2());
    auto* limit = plot.addHorizontalLine(500.0);
    limit->setYAxis(plot.yAxis2());
    limit->setIncludedInAutoscale(true);
    EXPECT_LT(plot.yAxis()->max(), 11.0);
    EXPECT_GT(plot.yAxis2()->max(), 500.0);

    // A log axis can't show zero or less: such an annotation is left out.
    auto* floor = plot.addHorizontalLine(-5.0);
    floor->setIncludedInAutoscale(true);
    plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_GT(plot.yAxis()->min(), 0.5);
    auto* band = plot.addHorizontalSpan(-1.0, 1000.0);
    band->setIncludedInAutoscale(true);
    EXPECT_GT(plot.yAxis()->min(), 0.5);
    EXPECT_GT(plot.yAxis()->max(), 1000.0);
}

TEST(Annotation, FollowingTheLatestDataIgnoresThem)
{
    PlotWidget plot;
    plot.addLine(std::vector<double>{0, 100}, std::vector<double>{0, 1});
    plot.xAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FOLLOW_LATEST);
    plot.xAxis()->setFollowWindow(10.0);
    auto* later = plot.addEvent(500.0, "planned");
    later->setIncludedInAutoscale(true);
    EXPECT_LT(plot.xAxis()->max(), 110.0);
}

TEST_F(AnnotationTest, AHiddenOneIsNotDrawn)
{
    const QImage plain = render();
    auto*        line  = m_plot.addVerticalLine(5.0, "here");
    line->setLineWidth(3.0);
    EXPECT_NE(render(), plain);
    line->setVisible(false);
    EXPECT_EQ(render(), plain);
}

TEST_F(AnnotationTest, TheLayerDecidesWhatCoversWhat)
{
    auto* data = m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{5, 5});
    data->setLineWidth(4.0);
    auto* span = m_plot.addVerticalSpan(4.0, 6.0);
    span->setColor(Qt::red);
    span->setOpacity(1.0);
    // Under the series by default: the line shows on it.
    EXPECT_EQ(render().pixelColor(pixelAt(5.0, 5.0)), data->color());
    EXPECT_EQ(render().pixelColor(pixelAt(5.0, 7.0)), QColor(Qt::red));
    span->setLayer(AnnotationLayer::ABOVE_SERIES);
    EXPECT_EQ(render().pixelColor(pixelAt(5.0, 5.0)), QColor(Qt::red));
}

TEST_F(AnnotationTest, AnnotationAtFindsWhatThePointerCanBeOn)
{
    Annotation* icon  = m_plot.addIcon(2.0, 2.0, redIcon());
    Annotation* note  = m_plot.addText(5.0, 5.0, "note");
    Annotation* event = m_plot.addEvent(8.0, "Burnout");
    Annotation* line  = m_plot.addHorizontalLine(3.0, "limit");     // labeled above its right end
    Annotation* span  = m_plot.addVerticalSpan(0.5, 1.5, "phase");  // labeled at its bottom left
    static_cast<void>(render());
    EXPECT_EQ(m_plot.annotationAt(pixelAt(2.0, 2.0)), icon);
    EXPECT_EQ(m_plot.annotationAt(pixelAt(5.0, 5.0)), note);
    const QPointF flag(m_plot.mapFromData(QPointF(8.0, 0.0)).x() + 6.0,
                       m_plot.plotArea().top() + 6.0);
    EXPECT_EQ(m_plot.annotationAt(flag), event);
    EXPECT_EQ(m_plot.annotationAt(pixelAt(10.0, 3.0, QPoint(-12, -9))), line);
    EXPECT_EQ(m_plot.annotationAt(pixelAt(0.5, 0.0, QPoint(10, -8))), span);
    // Not the line itself, nor anywhere else.
    EXPECT_EQ(m_plot.annotationAt(pixelAt(4.0, 3.0)), nullptr);
    EXPECT_EQ(m_plot.annotationAt(pixelAt(3.5, 7.0)), nullptr);

    // Where two overlap, the one on top: drawn later.
    Annotation* over = m_plot.addText(5.0, 5.0, "over");
    static_cast<void>(render());
    EXPECT_EQ(m_plot.annotationAt(pixelAt(5.0, 5.0)), over);
    // A removed one is gone at once; a hidden one once drawn again.
    m_plot.removeAnnotation(over);
    EXPECT_EQ(m_plot.annotationAt(pixelAt(5.0, 5.0)), note);
    note->setVisible(false);
    static_cast<void>(render());
    EXPECT_EQ(m_plot.annotationAt(pixelAt(5.0, 5.0)), nullptr);
}

TEST_F(AnnotationTest, TheLegendCoversThem)
{
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{0, 10}, "up");
    m_plot.addLine(std::vector<double>{0, 10}, std::vector<double>{10, 0}, "down");
    m_plot.legend()->setAnchor(rocketplot::LegendAnchor::TOP_LEFT);
    static_cast<void>(render());
    const QPointF under = m_plot.legendArea().center();
    m_plot.addIcon(m_plot.mapToData(under).x(), m_plot.mapToData(under).y(), redIcon());
    static_cast<void>(render());
    EXPECT_EQ(m_plot.annotationAt(under), nullptr);
}

TEST_F(PointingTest, AToolTipOverIt)
{
    Annotation* note = m_plot.addText(5.0, 5.0, "note");
    note->setToolTip("A <b>note</b>");
    m_plot.addIcon(2.0, 2.0, redIcon());  // without a tool tip
    static_cast<void>(render());
    const auto help = [this](QPoint position) {
        QHelpEvent event(QEvent::ToolTip, position, m_plot.mapToGlobal(position));
        QApplication::sendEvent(&m_plot, &event);
    };
    help(pixelAt(5.0, 5.0));
    EXPECT_TRUE(QToolTip::isVisible());
    EXPECT_EQ(QToolTip::text(), "A <b>note</b>");
    QToolTip::hideText();  // after a moment
    ASSERT_TRUE(QTest::qWaitFor([] { return !QToolTip::isVisible(); }));
    help(pixelAt(2.0, 2.0));
    EXPECT_FALSE(QToolTip::isVisible());
}

TEST_F(PointingTest, HoveringSignalsOnAndOff)
{
    Annotation* icon = m_plot.addIcon(2.0, 2.0, redIcon());
    static_cast<void>(render());
    QTest::mouseMove(&m_plot, QPoint(1, 1));
    const QSignalSpy hovered(&m_plot, &PlotWidget::annotationHovered);
    QTest::mouseMove(&m_plot, pixelAt(2.0, 2.0));
    ASSERT_EQ(hovered.count(), 1);
    EXPECT_EQ(qvariant_cast<Annotation*>(hovered.at(0).at(0)), icon);
    QTest::mouseMove(&m_plot, pixelAt(2.0, 2.0, QPoint(2, 2)));  // still on it
    EXPECT_EQ(hovered.count(), 1);
    QTest::mouseMove(&m_plot, pixelAt(6.0, 6.0));
    ASSERT_EQ(hovered.count(), 2);
    EXPECT_EQ(qvariant_cast<Annotation*>(hovered.at(1).at(0)), nullptr);
    // Removing the one the pointer is on is moving off it.
    QTest::mouseMove(&m_plot, pixelAt(2.0, 2.0));
    m_plot.removeAnnotation(icon);
    ASSERT_EQ(hovered.count(), 4);
    EXPECT_EQ(qvariant_cast<Annotation*>(hovered.at(3).at(0)), nullptr);
}

}  // namespace
