#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

class ErrorData;
class SeriesData;

/// A column of numbers a series adds after its y values and errors, one per point.
struct CsvColumn
{
    std::string             suffix;  ///< Follows the series' name in the header: "(bin start)"
    std::span<const double> values;
};

/// One series of a CSV table.
struct CsvSeries
{
    std::string       name;            ///< Heads its column of y values
    const SeriesData* data = nullptr;  ///< Its points
    const ErrorData*  errors =
        nullptr;  ///< Its errors, written as the ends of the bars; may be null
    /// More numbers per point, after the errors.
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::vector<CsvColumn> columns = {};
    /// What each point is, as text in a last column "<series> (part)": "median", "outlier".
    // NOLINTNEXTLINE(readability-redundant-member-init): lets designated initializers leave it out
    std::span<const std::string> labels = {};
};

/// What a CSV table holds and how it is written.
struct CsvTable
{
    std::span<const CsvSeries> series;
    Range                      xRange;  ///< Only points whose x is in this range are written
    std::string                xName     = "x";  ///< Heads the column of x values
    char                       separator = ',';
};

/// Writes @p table as CSV text, passed to @p write a piece at a time (so that millions of rows
/// needn't be held in memory): a header row, then one row per point.
///
/// Series with the same x values in the range (channels on one time base) share an x column, which
/// their y columns follow. A series with other x values gets an x column of its own, headed
/// "<xName> (<series>)", after those; where one runs out of points before another, its cells are
/// empty. A series with errors has two more columns per axis, "<series> (low)" and
/// "<series> (high)" for y and "<series> x (low)" and "<series> x (high)" for x; then come its
/// own columns ("<series> <suffix>") and its labels. A point past the end of a column of its own
/// has an empty cell there.
///
/// Numbers are written in the shortest form that reads back as the same double, with a decimal
/// point whatever the locale; a NaN is an empty cell. Names with a separator, a quote or a line
/// break in them are quoted (RFC 4180). Lines end in "\n".
void writeCsv(const CsvTable& table, const std::function<void(std::string_view)>& write);

}  // namespace rocketplot::core
