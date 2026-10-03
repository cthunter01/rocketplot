#include "core/CsvWriter.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/ErrorData.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace
{

using rocketplot::Range;
using rocketplot::core::CsvSeries;
using rocketplot::core::CsvTable;
using rocketplot::core::ErrorData;
using rocketplot::core::SeriesData;
using rocketplot::core::writeCsv;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr Range  kEverything{.min = -1e300, .max = 1e300};

SeriesData points(std::vector<double> x, std::vector<double> y)
{
    SeriesData data;
    data.setOwned(std::move(x), std::move(y));
    return data;
}

std::string csvOf(const CsvTable& table)
{
    std::string text;
    writeCsv(table, [&](std::string_view piece) { text += piece; });
    return text;
}

TEST(CsvWriter, SeriesOnTheSameXShareOneColumn)
{
    const SeriesData             altitude = points({0, 1, 2}, {10, 20, 30});
    const SeriesData             velocity = points({0, 1, 2}, {0.5, 1.5, 2.5});
    const std::vector<CsvSeries> series{
        {.name = "Altitude", .data = &altitude},
        {.name = "Velocity", .data = &velocity},
    };
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything, .xName = "Time (s)"}),
              "Time (s),Altitude,Velocity\n"
              "0,10,0.5\n"
              "1,20,1.5\n"
              "2,30,2.5\n");
}

TEST(CsvWriter, SeriesOnDifferentXEachHaveTheirOwn)
{
    const SeriesData             coarse = points({0, 10}, {1, 2});
    const SeriesData             fine   = points({0, 5, 10}, {3, 4, 5});
    const std::vector<CsvSeries> series{
        {.name = "coarse", .data = &coarse},
        {.name = "fine", .data = &fine},
    };
    // The shorter series leaves its cells empty.
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything, .xName = "t"}),
              "t,coarse,t (fine),fine\n"
              "0,1,0,3\n"
              "10,2,5,4\n"
              ",,10,5\n");
}

TEST(CsvWriter, SeriesAreGroupedByTheirX)
{
    // Two channels on one time base, and a few points measured at other times between them.
    const SeriesData             altitude = points({0, 1, 2}, {10, 20, 30});
    const SeriesData             fixes    = points({0.5, 1.5}, {12, 26});
    const SeriesData             velocity = points({0, 1, 2}, {0.5, 1.5, 2.5});
    const std::vector<CsvSeries> series{
        {.name = "Altitude", .data = &altitude},
        {.name = "Fixes", .data = &fixes},
        {.name = "Velocity", .data = &velocity},
    };
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything, .xName = "Time (s)"}),
              "Time (s),Altitude,Velocity,Time (s) (Fixes),Fixes\n"
              "0,10,0.5,0.5,12\n"
              "1,20,1.5,1.5,26\n"
              "2,30,2.5,,\n");
}

TEST(CsvWriter, OnlyThePointsInTheXRange)
{
    SeriesData samples;
    samples.setOwned(rocketplot::UniformX{.start = 0.0, .step = 0.5},
                     std::vector<double>{0, 1, 2, 3, 4});
    // Not sorted by x: every point is looked at, and they keep their order.
    const SeriesData             loop = points({2, 0.5, 1.5, 9, 1}, {1, 2, 3, 4, 5});
    const std::vector<CsvSeries> one{{.name = "samples", .data = &samples}};
    const std::vector<CsvSeries> other{{.name = "loop", .data = &loop}};
    const Range                  range{.min = 0.5, .max = 1.5};
    EXPECT_EQ(csvOf({.series = one, .xRange = range}), "x,samples\n0.5,1\n1,2\n1.5,3\n");
    EXPECT_EQ(csvOf({.series = other, .xRange = range}), "x,loop\n0.5,2\n1.5,3\n1,5\n");
    // Nothing in range: the header alone.
    EXPECT_EQ(csvOf({.series = one, .xRange = {.min = 50.0, .max = 60.0}}), "x,samples\n");
}

TEST(CsvWriter, NumbersReadBackExactlyAndGapsAreEmpty)
{
    const SeriesData             data = points({0.1, 1e-300, 3}, {1.0 / 3.0, kNaN, -2.5e17});
    const std::vector<CsvSeries> series{{.name = "y", .data = &data}};
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything}),
              "x,y\n"
              "0.1,0.3333333333333333\n"
              "1e-300,\n"
              "3,-2.5e+17\n");
}

TEST(CsvWriter, ErrorsAreWrittenAsTheEndsOfTheBars)
{
    SeriesData                data = points({1, 2}, {10, 20});
    ErrorData                 errors;
    const std::vector<double> below{1, 2};
    const std::vector<double> above{3, 4};
    const std::vector<double> across{0.5, 0.5};
    errors.setY(data, below, above);
    errors.setX(data, across, across);
    // A point added since has no errors.
    const std::vector<double> moreX{3};
    const std::vector<double> moreY{30};
    data.append(moreX, moreY);
    const std::vector<CsvSeries> series{{.name = "thrust", .data = &data, .errors = &errors}};
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything}),
              "x,thrust,thrust x (low),thrust x (high),thrust (low),thrust (high)\n"
              "1,10,0.5,1.5,9,13\n"
              "2,20,1.5,2.5,18,24\n"
              "3,30,,,,\n");
}

TEST(CsvWriter, NamesAreQuotedWhereTheyMust)
{
    const SeriesData             data = points({1}, {2});
    const std::vector<CsvSeries> series{{.name = "Thrust, \"vacuum\"", .data = &data}};
    EXPECT_EQ(csvOf({.series = series, .xRange = kEverything, .xName = "Time\n(s)"}),
              "\"Time\n(s)\",\"Thrust, \"\"vacuum\"\"\"\n1,2\n");
    // With another separator, a comma needs no quotes.
    const std::vector<CsvSeries> plain{{.name = "a,b", .data = &data}};
    EXPECT_EQ(csvOf({.series = plain, .xRange = kEverything, .separator = ';'}), "x;a,b\n1;2\n");
}

TEST(CsvWriter, NothingWithoutSeries)
{
    EXPECT_EQ(csvOf({.series = {}, .xRange = kEverything}), "");
}

TEST(CsvWriter, LongTablesArriveInPieces)
{
    constexpr std::size_t kCount = 200000;
    SeriesData            data;
    data.setOwned(rocketplot::UniformX{}, std::vector<double>(kCount, 1.5));
    const std::vector<CsvSeries> series{{.name = "y", .data = &data}};
    std::size_t                  pieces = 0;
    std::size_t                  lines  = 0;
    writeCsv({.series = series, .xRange = kEverything}, [&](std::string_view piece) {
        ++pieces;
        lines += static_cast<std::size_t>(std::ranges::count(piece, '\n'));
        EXPECT_EQ(piece.back(), '\n');  // whole rows
    });
    EXPECT_GT(pieces, 10U);
    EXPECT_EQ(lines, kCount + 1);
}

}  // namespace
