#include "rocketplot/HistogramSeries.h"

#include <QJsonObject>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/BarSeries.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::HistogramNormalization;
using rocketplot::HistogramSeries;
using rocketplot::PlotWidget;
using rocketplot::Range;

std::vector<double> oneToHundred()
{
    std::vector<double> values;
    values.reserve(100);
    for (int value = 1; value <= 100; ++value)
    {
        values.push_back(value);
    }
    return values;
}

double total(const HistogramSeries& histogram)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < histogram.size(); ++i)
    {
        sum += histogram.y(i);
    }
    return sum;
}

TEST(HistogramSeries, CountsItsValuesIntoAutomaticBins)
{
    PlotWidget             plot;
    const HistogramSeries* histogram = plot.addHistogram(oneToHundred(), "Counts");
    EXPECT_EQ(histogram->kind(), rocketplot::SeriesKind::BARS);
    EXPECT_EQ(histogram->size(), 10U);  // the square root of 100
    EXPECT_EQ(histogram->binCount(), 0);
    EXPECT_FALSE(histogram->hasBinRange());
    EXPECT_EQ(histogram->binRange(), (Range{.min = 1.0, .max = 100.0}));
    EXPECT_DOUBLE_EQ(total(*histogram), 100.0);
    EXPECT_EQ(histogram->values().size(), 100U);
    EXPECT_EQ(plot.yAxis()->min(), 0.0);
}

TEST(HistogramSeries, BinsAndRangeAsAsked)
{
    PlotWidget       plot;
    HistogramSeries* histogram =
        plot.addHistogram(oneToHundred(), {}, 4, {.min = 0.0, .max = 40.0});
    EXPECT_EQ(histogram->size(), 4U);
    EXPECT_EQ(histogram->binRange(), (Range{.min = 0.0, .max = 40.0}));
    EXPECT_DOUBLE_EQ(total(*histogram), 40.0);  // 1 to 40
    histogram->resetBinRange();
    histogram->setBinCount(20);
    EXPECT_EQ(histogram->size(), 20U);
    EXPECT_DOUBLE_EQ(total(*histogram), 100.0);
    // What can't be a count or a range is ignored.
    histogram->setBinCount(-3);
    histogram->setBinRange({.min = 5.0, .max = 5.0});
    EXPECT_EQ(histogram->binCount(), 20);
    EXPECT_FALSE(histogram->hasBinRange());
}

TEST(HistogramSeries, Normalization)
{
    PlotWidget       plot;
    HistogramSeries* histogram = plot.addHistogram(oneToHundred(), {}, 5, Range::empty(),
                                                   HistogramNormalization::PROBABILITY);
    EXPECT_DOUBLE_EQ(total(*histogram), 1.0);
    histogram->setNormalization(HistogramNormalization::DENSITY);
    double area = 0.0;
    for (std::size_t i = 0; i < histogram->size(); ++i)
    {
        area += histogram->y(i) * histogram->bin(i).span();
    }
    EXPECT_DOUBLE_EQ(area, 1.0);
}

TEST(HistogramSeries, ItsBarsComeFromItsValues)
{
    PlotWidget       plot;
    HistogramSeries* histogram = plot.addHistogram(oneToHundred());
    EXPECT_THROW(histogram->setBars(std::vector{0.0, 1.0}, std::vector{1.0}), std::logic_error);
    histogram->setValues(std::vector{1.0, 2.0, 2.5});
    EXPECT_DOUBLE_EQ(total(*histogram), 3.0);
}

TEST(HistogramSeries, ItsSettingsAreSaved)
{
    PlotWidget       plot;
    HistogramSeries* histogram = plot.addHistogram(oneToHundred(), "Counts");
    histogram->setBinCount(7);
    histogram->setBinRange({.min = 0.0, .max = 70.0});
    histogram->setNormalization(HistogramNormalization::DENSITY);
    PlotWidget       other;
    HistogramSeries* restored = other.addHistogram(oneToHundred(), "Counts");
    ASSERT_TRUE(other.restoreState(plot.saveState()));
    EXPECT_EQ(restored->binCount(), 7);
    EXPECT_EQ(restored->binRange(), (Range{.min = 0.0, .max = 70.0}));
    EXPECT_EQ(restored->normalization(), HistogramNormalization::DENSITY);
}

}  // namespace
