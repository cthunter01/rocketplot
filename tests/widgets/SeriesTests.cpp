#include "rocketplot/Series.h"

#include <QColor>
#include <QList>
#include <QPen>
#include <QSignalSpy>
#include <QVariant>
#include <Qt>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/LineSeries.h"
#include "rocketplot/NumericRange.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/ScatterSeries.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::LineSeries;
using rocketplot::Marker;
using rocketplot::PlotWidget;
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

}  // namespace
