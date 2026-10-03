#include "rocketplot/Annotation.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QSignalSpy>
#include <QVariant>
#include <Qt>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/EventMarker.h"
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

}  // namespace
