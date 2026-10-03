#include "rocketplot/Series.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QVariant>
#include <Qt>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "RenderedPlotTest.h"
#include "rocketplot/Axis.h"
#include "rocketplot/LineSeries.h"
#include "rocketplot/NumericRange.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::ErrorStyle;
using rocketplot::LineSeries;
using rocketplot::Marker;
using rocketplot::PlotWidget;
using rocketplot::Range;
using rocketplot::UniformX;

static_assert(rocketplot::NumericRange<std::vector<int>>);
static_assert(rocketplot::NumericRange<std::array<float, 3>>);
static_assert(rocketplot::NumericRange<std::span<const std::int16_t>>);
static_assert(rocketplot::NumericRange<QList<double>>);
static_assert(
    !rocketplot::NumericRange<std::remove_cvref_t<decltype("hello")>>);  // a name, not data
static_assert(!rocketplot::NumericRange<std::vector<bool>>);

TEST(Series, CopiesAnyNumericRange)
{
    PlotWidget                 plot;
    const std::vector<int>     x{1, 2, 3};
    const std::array<float, 3> y{0.5F, 1.5F, 2.5F};
    LineSeries*                line = plot.addLine(x, y, "ints and floats");
    ASSERT_EQ(line->size(), 3U);
    EXPECT_DOUBLE_EQ(line->x(2), 3.0);
    EXPECT_DOUBLE_EQ(line->y(0), 0.5);
    EXPECT_FALSE(line->isView());
    EXPECT_EQ(line->name(), "ints and floats");

    const QList<double> samples{4.0, 5.0};
    LineSeries*         indexed = plot.addLine(samples);
    EXPECT_TRUE(indexed->hasUniformX());
    EXPECT_DOUBLE_EQ(indexed->x(1), 1.0);
}

TEST(Series, MovesRvalueVectors)
{
    PlotWidget          plot;
    std::vector<double> x{1, 2, 3};
    std::vector<double> y{4, 5, 6};
    LineSeries*         line = plot.addLine(std::move(x), std::move(y));
    EXPECT_FALSE(line->isView());
    EXPECT_EQ(line->size(), 3U);
    EXPECT_DOUBLE_EQ(line->y(2), 6.0);
}

TEST(Series, UniformXFromSampleRate)
{
    PlotWidget                plot;
    const std::vector<double> samples(1000, 1.0);
    LineSeries* line = plot.addLine(UniformX{.start = 2.0, .step = 0.001}, samples, "1 kHz");
    EXPECT_TRUE(line->hasUniformX());
    EXPECT_NEAR(line->x(999), 2.999, 1e-12);
    EXPECT_NEAR(line->xBounds().max, 2.999, 1e-12);
}

TEST(Series, ViewsReadCallerMemory)
{
    PlotWidget          plot;
    std::vector<double> x{0, 1, 2};
    std::vector<double> y{0, 1, 2};
    LineSeries*         line = plot.addLineView(x, y);
    EXPECT_TRUE(line->isView());
    y[2] = 50.0;
    const QSignalSpy spy(line, &rocketplot::Series::dataChanged);
    line->notifyDataChanged();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(line->yBounds().max, 50.0);
}

TEST(Series, Append)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{0.0}, std::vector<double>{1.0});
    line->append(1.0, 2.0);
    line->append(std::vector<double>{2.0, 3.0}, std::vector<int>{3, 4});
    EXPECT_EQ(line->size(), 4U);
    EXPECT_DOUBLE_EQ(line->yBounds().max, 4.0);
    EXPECT_THROW(line->append(5.0), std::logic_error);  // has an x array: needs x values

    LineSeries* samples = plot.addLine(std::vector<double>{1.0, 2.0});
    samples->append(3.0);
    samples->append(std::array<double, 2>{4.0, 5.0});
    EXPECT_EQ(samples->size(), 5U);
    EXPECT_DOUBLE_EQ(samples->x(4), 4.0);
    EXPECT_THROW(samples->append(1.0, 1.0), std::logic_error);
}

TEST(Series, SizeMismatchThrows)
{
    PlotWidget plot;
    EXPECT_THROW(plot.addLine(std::vector<double>{1, 2}, std::vector<double>{1}),
                 std::invalid_argument);
    EXPECT_TRUE(plot.series().isEmpty());
    LineSeries* line = plot.addLine(std::vector<double>{1});
    EXPECT_THROW(line->setData(std::vector<int>{1, 2}, std::vector<int>{1}), std::invalid_argument);
}

TEST(Series, Defaults)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    LineSeries* line    = plot.addLine(std::vector<double>{1});
    const auto* scatter = plot.addScatter(std::vector<double>{1});
    EXPECT_EQ(line->marker(), Marker::NONE);
    EXPECT_EQ(scatter->marker(), Marker::CIRCLE);
    EXPECT_DOUBLE_EQ(line->lineWidth(), plot.theme().lineWidth);
    EXPECT_DOUBLE_EQ(scatter->markerSize(), plot.theme().markerSize);
    EXPECT_EQ(line->color(), plot.theme().seriesColor(0));
    EXPECT_EQ(scatter->color(), plot.theme().seriesColor(1));
}

TEST(Series, PenRoundTrip)
{
    PlotWidget       plot;
    LineSeries*      line = plot.addLine(std::vector<double>{1});
    const QSignalSpy spy(line, &rocketplot::Series::changed);
    line->setPen(QPen(QColor(Qt::red), 3.5, Qt::DashLine));
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(line->color(), QColor(Qt::red));
    EXPECT_DOUBLE_EQ(line->lineWidth(), 3.5);
    EXPECT_EQ(line->lineStyle(), Qt::DashLine);
    EXPECT_EQ(line->pen().capStyle(), Qt::RoundCap);
    line->resetColor();
    EXPECT_EQ(line->color(), plot.theme().seriesColor(0));
}

TEST(Series, PropertiesWorkThroughTheMetaObject)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{1});
    EXPECT_TRUE(line->setProperty("lineWidth", 4.0));
    EXPECT_DOUBLE_EQ(line->lineWidth(), 4.0);
    EXPECT_TRUE(line->setProperty("marker", QVariant::fromValue(Marker::DIAMOND)));
    EXPECT_EQ(line->marker(), Marker::DIAMOND);
    EXPECT_EQ(line->property("name").toString(), QString());
}

TEST(Series, ColorsFollowTheTheme)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{1});
    plot.setThemeMode(rocketplot::ThemeMode::DARK);
    EXPECT_EQ(line->color(), rocketplot::Theme::dark().seriesColor(0));
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    EXPECT_EQ(line->color(), rocketplot::Theme::light().seriesColor(0));
    line->setColor(Qt::black);
    plot.setThemeMode(rocketplot::ThemeMode::DARK);
    EXPECT_EQ(line->color(), QColor(Qt::black));  // a color set by the user stays
}

TEST(Series, NearestIndex)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{0, 1, 4}, std::vector<double>{5, 6, 7});
    EXPECT_EQ(line->nearestIndex(2.4), 1U);
    EXPECT_EQ(line->nearestIndex(2.6), 2U);
    EXPECT_FALSE(line->nearestIndex(4.5));  // past the data
    LineSeries* unsorted = plot.addLine(std::vector<double>{0, 2, 1}, std::vector<double>{5, 6, 7});
    EXPECT_FALSE(unsorted->nearestIndex(1.0));
}

TEST(Series, ErrorsAreSetPerPoint)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{0, 1, 2}, std::vector<double>{10, 20, 30});
    const QSignalSpy changed(line, &rocketplot::Series::dataChanged);
    EXPECT_FALSE(line->hasYErrors());
    EXPECT_EQ(line->yErrorRange(1), (Range{.min = 20.0, .max = 20.0}));

    line->setYErrors(std::vector<double>{1, 2, 3});
    EXPECT_TRUE(line->hasYErrors());
    EXPECT_FALSE(line->hasXErrors());
    EXPECT_EQ(line->yErrorRange(1), (Range{.min = 18.0, .max = 22.0}));
    EXPECT_EQ(changed.count(), 1);

    // Different below and above, from any ranges of numbers.
    line->setYErrors(std::vector<int>{1, 1, 1}, std::array<float, 3>{0.5F, 4.0F, 0.5F});
    EXPECT_EQ(line->yErrorRange(1), (Range{.min = 19.0, .max = 24.0}));
    line->setXErrors(std::array<double, 3>{0.25, 0.25, 0.25});
    EXPECT_TRUE(line->hasXErrors());
    EXPECT_EQ(line->xErrorRange(2), (Range{.min = 1.75, .max = 2.25}));
    line->setXErrors(std::vector<double>{0.5, 0.5, 0.5}, std::vector<int>{1, 1, 1});
    EXPECT_EQ(line->xErrorRange(0), (Range{.min = -0.5, .max = 1.0}));
    EXPECT_EQ(line->xBounds(), (Range{.min = 0.0, .max = 2.0}));  // of the points themselves

    line->clearErrors();
    EXPECT_FALSE(line->hasXErrors());
    EXPECT_FALSE(line->hasYErrors());
    EXPECT_EQ(changed.count(), 5);
    line->clearErrors();  // nothing to clear
    EXPECT_EQ(changed.count(), 5);
}

TEST(Series, ErrorsNeedOneValuePerPoint)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{0, 1, 2});
    EXPECT_THROW(line->setYErrors(std::vector<double>{1, 2}), std::invalid_argument);
    EXPECT_THROW(line->setYErrors(std::vector<double>{1, 2, 3}, std::vector<double>{1}),
                 std::invalid_argument);
    EXPECT_THROW(line->setXErrors(std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
    EXPECT_FALSE(line->hasYErrors());
    EXPECT_FALSE(line->hasXErrors());
}

TEST(Series, ErrorsGoWithThePointsTheyWereSetFor)
{
    PlotWidget  plot;
    LineSeries* line = plot.addLine(std::vector<double>{0, 1}, std::vector<double>{10, 20});
    line->setYErrors(std::vector<double>{1, 2});
    // A point added later has none.
    line->append(2.0, 30.0);
    EXPECT_TRUE(line->hasYErrors());
    EXPECT_EQ(line->yErrorRange(1), (Range{.min = 18.0, .max = 22.0}));
    EXPECT_EQ(line->yErrorRange(2), (Range{.min = 30.0, .max = 30.0}));
    // New data: the errors of the old data are gone.
    line->setData(std::vector<double>{0, 1}, std::vector<double>{100, 200});
    EXPECT_FALSE(line->hasYErrors());

    std::vector<double> x{0, 1};
    std::vector<double> y{1, 2};
    LineSeries*         view = plot.addLineView(x, y);
    view->setXErrors(std::vector<double>{1, 1});
    y[0] = 5.0;
    view->notifyDataChanged();
    EXPECT_FALSE(view->hasXErrors());
}

TEST(Series, ErrorStyleAndSizes)
{
    PlotWidget plot;
    plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
    LineSeries* line    = plot.addLine(std::vector<double>{1});
    auto*       scatter = plot.addScatter(std::vector<double>{1});
    EXPECT_EQ(line->errorStyle(), ErrorStyle::BAND);
    EXPECT_EQ(scatter->errorStyle(), ErrorStyle::BARS);
    EXPECT_DOUBLE_EQ(line->bandOpacity(), plot.theme().bandOpacity);
    EXPECT_DOUBLE_EQ(scatter->errorCapSize(), plot.theme().errorCapSize);

    const QSignalSpy changed(scatter, &rocketplot::Series::changed);
    scatter->setErrorStyle(ErrorStyle::BAND);
    scatter->setErrorCapSize(0.0);  // no caps
    scatter->setBandOpacity(7.0);   // as opaque as it gets
    EXPECT_EQ(scatter->errorStyle(), ErrorStyle::BAND);
    EXPECT_DOUBLE_EQ(scatter->errorCapSize(), 0.0);
    EXPECT_DOUBLE_EQ(scatter->bandOpacity(), 1.0);
    EXPECT_EQ(changed.count(), 3);
    scatter->setErrorCapSize(-1.0);  // ignored
    EXPECT_DOUBLE_EQ(scatter->errorCapSize(), 0.0);
    scatter->resetErrorCapSize();
    scatter->resetBandOpacity();
    EXPECT_DOUBLE_EQ(scatter->errorCapSize(), plot.theme().errorCapSize);
    EXPECT_DOUBLE_EQ(scatter->bandOpacity(), plot.theme().bandOpacity);
    EXPECT_TRUE(line->setProperty("errorStyle", QVariant::fromValue(ErrorStyle::BARS)));
    EXPECT_EQ(line->errorStyle(), ErrorStyle::BARS);
}

using SeriesErrorsTest = rocketplot::test::RenderedPlotTest;

// A place in a rendering, and whether the errors are drawn there.
struct Probe
{
    double x;
    double y;
    QPoint offset;
    bool   drawn;
};

TEST_F(SeriesErrorsTest, ScatterErrorsAreBars)
{
    auto*        points = m_plot.addScatter(std::vector<double>{5}, std::vector<double>{5});
    const QImage plain  = render();
    points->setYErrors(std::vector<double>{2}, std::vector<double>{3});
    points->setXErrors(std::vector<double>{1});
    const QImage bars   = render();
    const auto   probes = std::to_array<Probe>({
        {.x = 5.0, .y = 7.0, .offset = {}, .drawn = true},  // the whisker above the point
        {.x = 5.0, .y = 3.5, .offset = {}, .drawn = true},  // and below it
        {.x = 5.0, .y = 8.5, .offset = {}, .drawn = false},
        {.x = 5.0, .y = 2.5, .offset = {}, .drawn = false},
        {.x = 5.5, .y = 5.0, .offset = {}, .drawn = true},  // the x error
        {.x = 6.5, .y = 5.0, .offset = {}, .drawn = false},
        {.x = 5.0, .y = 8.0, .offset = QPoint(2, 0), .drawn = true},  // a cap at the end
        {.x = 5.0, .y = 7.0, .offset = QPoint(2, 0), .drawn = false},
    });
    for (const Probe& probe : probes)
    {
        const QPoint pixel = pixelAt(probe.x, probe.y, probe.offset);
        EXPECT_EQ(bars.pixelColor(pixel) != plain.pixelColor(pixel), probe.drawn)
            << probe.x << ", " << probe.y;
    }
}

TEST_F(SeriesErrorsTest, BarsHaveNoCapsAtSizeZero)
{
    auto*        points = m_plot.addScatter(std::vector<double>{5}, std::vector<double>{5});
    const QImage plain  = render();
    points->setYErrors(std::vector<double>{3});
    points->setErrorCapSize(0.0);
    const QImage bare    = render();
    const QPoint capEnd  = pixelAt(5.0, 8.0, QPoint(2, 0));
    const QPoint whisker = pixelAt(5.0, 7.0);
    EXPECT_EQ(bare.pixelColor(capEnd), plain.pixelColor(capEnd));
    EXPECT_NE(bare.pixelColor(whisker), plain.pixelColor(whisker));
}

TEST_F(SeriesErrorsTest, LineErrorsAreABand)
{
    LineSeries*  line = m_plot.addLine(std::vector<double>{0, 5, 10}, std::vector<double>{5, 5, 5});
    const QImage plain = render();
    line->setYErrors(std::vector<double>{2, 2, 2});
    const QImage band = render();
    // Shaded between the points, a wash of the line's color; nothing outside it.
    const QPoint inside = pixelAt(2.5, 6.3);
    EXPECT_NE(band.pixelColor(inside), plain.pixelColor(inside));
    EXPECT_NE(band.pixelColor(inside), line->color());
    EXPECT_GT(band.pixelColor(inside).lightnessF(), 0.8F);
    EXPECT_EQ(band.pixelColor(pixelAt(2.5, 7.5)), plain.pixelColor(pixelAt(2.5, 7.5)));
    EXPECT_EQ(band.pixelColor(pixelAt(2.5, 2.5)), plain.pixelColor(pixelAt(2.5, 2.5)));

    // As bars instead: nothing between the points, a whisker at each.
    line->setErrorStyle(ErrorStyle::BARS);
    const QImage bars = render();
    EXPECT_EQ(bars.pixelColor(inside), plain.pixelColor(inside));
    EXPECT_NE(bars.pixelColor(pixelAt(5.0, 6.3)), plain.pixelColor(pixelAt(5.0, 6.3)));
}

TEST_F(SeriesErrorsTest, BarsTooCloseToTellApartBecomeABand)
{
    std::vector<double> x;
    for (int i = 0; i <= 5000; ++i)
    {
        x.push_back(i / 500.0);
    }
    auto* points = m_plot.addScatter(x, std::vector<double>(x.size(), 5.0));
    points->setYErrors(std::vector<double>(x.size(), 2.0));
    const QColor inBand = render().pixelColor(pixelAt(5.0, 6.3));
    EXPECT_NE(inBand, m_plot.theme().background);
    EXPECT_GT(inBand.lightnessF(), 0.8F);  // a wash, not a wall of whiskers

    // Zoomed in until the points are apart, they are bars again.
    m_plot.xAxis()->setRange(5.0, 5.2);
    const QImage zoomed = render();
    EXPECT_EQ(zoomed.pixelColor(pixelAt(5.051, 6.3)).rgb(), m_plot.theme().background.rgb());
}

TEST_F(SeriesErrorsTest, AwkwardErrorsRender)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    LineSeries*  gaps =
        m_plot.addLine(std::vector<double>{1, 2, 3, 4, 5}, std::vector<double>{1, nan, 3, nan, 5});
    gaps->setYErrors(std::vector<double>{1, 1, nan, 1, 1e300});
    gaps->setXErrors(std::vector<double>{-1, 1, 1, 1, std::numeric_limits<double>::infinity()});
    auto* unsorted = m_plot.addLine(std::vector<double>{3, 1, 2}, std::vector<double>{1, 2, 3});
    unsorted->setYErrors(std::vector<double>{1, 1, 1});  // a band needs sorted x: bars
    m_plot.addScatter(std::vector<double>{})->setYErrors(std::vector<double>{});
    m_plot.setDebugOverlay(true);
    EXPECT_FALSE(m_plot.grab().isNull());

    // On log axes, where bars reach down to zero and below.
    m_plot.resetView();
    m_plot.xAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    m_plot.yAxis()->setScaleType(rocketplot::ScaleType::LOGARITHMIC);
    EXPECT_GT(m_plot.yAxis()->min(), 0.0);
    EXPECT_FALSE(m_plot.grab().isNull());
    gaps->setErrorStyle(ErrorStyle::BARS);
    EXPECT_FALSE(m_plot.grab().isNull());
}

TEST(Series, AutoscaleMakesRoomForErrors)
{
    PlotWidget plot;
    auto*      points = plot.addScatter(std::vector<double>{0, 10}, std::vector<double>{0, 10});
    EXPECT_GT(plot.yAxis()->min(), -1.0);
    points->setYErrors(std::vector<double>{5, 1}, std::vector<double>{1, 20});
    points->setXErrors(std::vector<double>{3, 3});
    EXPECT_LT(plot.yAxis()->min(), -5.0);
    EXPECT_GT(plot.yAxis()->max(), 30.0);
    EXPECT_LT(plot.xAxis()->min(), -3.0);
    EXPECT_GT(plot.xAxis()->max(), 13.0);

    // Fitting what's in view counts the errors of the points in view.
    plot.yAxis()->setAutoscaleMode(rocketplot::AutoscaleMode::FIT_VISIBLE);
    plot.xAxis()->setRange(-1.0, 1.0);
    EXPECT_LT(plot.yAxis()->min(), -5.0);
    EXPECT_LT(plot.yAxis()->max(), 2.0);

    points->clearErrors();
    plot.resetView();
    EXPECT_GT(plot.xAxis()->min(), -1.0);
}

}  // namespace
