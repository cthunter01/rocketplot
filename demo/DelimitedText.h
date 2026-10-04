#pragma once

#include <QByteArrayView>
#include <QString>
#include <cstddef>
#include <vector>

namespace rocketplot::demo
{

/// A table of numbers read from text: what a CSV file or a selection copied from a spreadsheet
/// holds.
struct DataTable
{
    struct Column
    {
        QString name;
        /// One value per row; NaN where the cell is empty or doesn't hold a number.
        std::vector<double> values;
        std::size_t         numbers = 0;  ///< Cells that hold a number
        /// Whether the cells hold dates and times: the values are seconds since 1970-01-01 00:00
        /// UTC, what a DATE_TIME axis takes.
        bool isTime = false;

        /// Whether any cell holds a number (or a time): a column worth plotting.
        [[nodiscard]] bool isNumeric() const noexcept { return numbers > 0; }
    };

    std::vector<Column> columns;
    std::size_t         rows      = 0;
    char                separator = ',';  ///< What separates the fields: , ; a tab or a space
    bool                hasHeader = false;

    /// Whether there is anything to plot.
    [[nodiscard]] bool isEmpty() const noexcept;
};

/// Reads a table from delimited text.
///
/// - Fields are separated by tabs, semicolons, commas or runs of spaces: whichever of them, in
///   that order, splits the first lines into the same number of fields.
/// - A first line without any number in it names the columns; otherwise they are "Column 1",
///   "Column 2", ...
/// - A field may be in double quotes, with "" for a quote inside it (but not span lines).
/// - Numbers are written with a decimal point; where commas don't separate the fields, with a
///   decimal comma too ("1,5").
/// - ISO 8601 dates and times ("2026-10-03T12:30:00Z", "2026-10-03 12:30:00.250", "2026-10-03")
///   become seconds since the epoch; without a zone they are taken as UTC.
/// - Empty lines and lines starting with # are skipped. Rows may be of different lengths: the
///   missing cells are gaps.
[[nodiscard]] DataTable parseDelimitedText(QByteArrayView text);

}  // namespace rocketplot::demo
