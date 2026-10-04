#include "DelimitedText.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QDateTime>
#include <QLatin1String>
#include <QString>
#include <QTimeZone>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "rocketplot/plottime.h"

namespace rocketplot::demo
{

namespace
{

constexpr double      kGap         = std::numeric_limits<double>::quiet_NaN();
constexpr std::size_t kSampleLines = 20;   // lines looked at to tell what separates the fields
constexpr char        kSpaces      = ' ';  // "separator": runs of spaces and tabs
constexpr qsizetype   kDateLength  = 10;   // yyyy-mm-dd

using Fields = std::vector<QByteArrayView>;

// The lines of a text that hold something: not the empty ones, nor comments.
class LineReader
{
public:
    explicit LineReader(QByteArrayView text) : m_text(text) { }

    // The next line, without its line end; false at the end of the text.
    bool next(QByteArrayView& line)
    {
        while (m_position < m_text.size())
        {
            qsizetype end = m_text.indexOf('\n', m_position);
            if (end < 0)
            {
                end = m_text.size();
            }
            line       = m_text.sliced(m_position, end - m_position);
            m_position = end + 1;
            if (line.endsWith('\r'))
            {
                line.chop(1);
            }
            const QByteArrayView content = line.trimmed();
            if (!content.isEmpty() && !content.startsWith('#'))
            {
                return true;
            }
        }
        return false;
    }

private:
    QByteArrayView m_text;
    qsizetype      m_position = 0;
};

bool isSpace(char character)
{
    return character == ' ' || character == '\t';
}

void splitAtSpaces(QByteArrayView line, Fields& fields)
{
    qsizetype position = 0;
    while (position < line.size())
    {
        while (position < line.size() && isSpace(line[position]))
        {
            ++position;
        }
        qsizetype end = position;
        while (end < line.size() && !isSpace(line[end]))
        {
            ++end;
        }
        if (end > position)
        {
            fields.push_back(line.sliced(position, end - position));
        }
        position = end;
    }
}

// The end of the quoted field whose opening quote is at @p start: the position of its closing
// quote (a doubled quote is a quote inside the field), or the end of the line without one.
qsizetype closingQuote(QByteArrayView line, qsizetype start)
{
    qsizetype end = start + 1;
    while (end < line.size())
    {
        if (line[end] != '"')
        {
            ++end;
        }
        else if (end + 1 < line.size() && line[end + 1] == '"')
        {
            end += 2;
        }
        else
        {
            break;
        }
    }
    return end;
}

// The fields of @p line: for a quoted one, what is between its quotes.
void split(QByteArrayView line, char separator, Fields& fields)
{
    fields.clear();
    if (separator == kSpaces)
    {
        splitAtSpaces(line, fields);
        return;
    }
    qsizetype position = 0;
    while (position >= 0)
    {
        while (position < line.size() && line[position] == ' ')
        {
            ++position;
        }
        qsizetype next = -1;
        if (position < line.size() && line[position] == '"')
        {
            const qsizetype end = closingQuote(line, position);
            fields.push_back(line.sliced(position + 1, end - position - 1));
            next = line.indexOf(separator, std::min(end + 1, line.size()));
        }
        else
        {
            next                = line.indexOf(separator, position);
            const qsizetype end = next < 0 ? line.size() : next;
            fields.push_back(line.sliced(position, end - position).trimmed());
        }
        position = next < 0 ? next : next + 1;
    }
}

// What separates the fields of @p lines: the first of tab, semicolon and comma that splits every
// line into the same number of fields; else the one that splits the most lines, if most of them;
// else spaces.
char separatorOf(const Fields& lines)
{
    constexpr std::array kCandidates{'\t', ';', ','};
    char                 best      = kSpaces;
    std::size_t          bestSplit = 0;  // lines the best candidate splits
    Fields               fields;
    for (const char candidate : kCandidates)
    {
        std::size_t fewest = std::numeric_limits<std::size_t>::max();
        std::size_t most   = 0;
        std::size_t split  = 0;
        for (const QByteArrayView line : lines)
        {
            demo::split(line, candidate, fields);
            fewest = std::min(fewest, fields.size());
            most   = std::max(most, fields.size());
            split += fields.size() > 1 ? 1U : 0U;
        }
        if (fewest > 1 && fewest == most)
        {
            return candidate;
        }
        if (split > bestSplit)
        {
            best      = candidate;
            bestSplit = split;
        }
    }
    return bestSplit * 2 > lines.size() ? best : kSpaces;
}

std::optional<double> toNumber(QByteArrayView field, bool decimalComma)
{
    bool   isNumber = false;
    double value    = field.toDouble(&isNumber);
    if (!isNumber && decimalComma && field.count(',') == 1 && !field.contains('.'))
    {
        value = field.toByteArray().replace(',', '.').toDouble(&isNumber);
    }
    return isNumber && std::isfinite(value) ? std::optional(value) : std::nullopt;
}

std::optional<double> toTime(QByteArrayView field)
{
    if (field.size() < kDateLength || field[4] != '-' || field[7] != '-')
    {
        return std::nullopt;
    }
    QString text = QString::fromLatin1(field);
    if (text.size() > kDateLength && text.at(kDateLength) == QLatin1Char(' '))
    {
        text[kDateLength] = QLatin1Char('T');
    }
    QDateTime time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid())
    {
        return std::nullopt;
    }
    if (time.timeSpec() == Qt::LocalTime)  // no zone given
    {
        time.setTimeZone(QTimeZone::UTC);
    }
    return toPlotTime(time);
}

// Whether the fields of the first line are names: there is one, and none is a number or a time.
bool areNames(const Fields& fields, bool decimalComma)
{
    bool named = false;
    for (const QByteArrayView field : fields)
    {
        if (toNumber(field, decimalComma) || toTime(field))
        {
            return false;
        }
        named = named || !field.isEmpty();
    }
    return named;
}

// Fills a table a row at a time.
class TableBuilder
{
public:
    TableBuilder(char separator, std::size_t expectedRows) : m_expectedRows(expectedRows)
    {
        m_table.separator = separator;
    }

    void setNames(const Fields& fields)
    {
        for (const QByteArrayView field : fields)
        {
            column(m_table.columns.size()).name =
                QString::fromUtf8(field).replace(QLatin1String("\"\""), QLatin1String("\""));
        }
        m_table.hasHeader = true;
    }

    void addRow(const Fields& fields)
    {
        const bool decimalComma = m_table.separator != ',';
        if (!fields.empty())
        {
            column(fields.size() - 1);  // a longer row than any before: more columns
        }
        for (std::size_t i = 0; i < m_table.columns.size(); ++i)
        {
            double value = kGap;
            if (i < fields.size() && !fields[i].isEmpty())
            {
                if (const std::optional<double> number = toNumber(fields[i], decimalComma))
                {
                    value = *number;
                    ++m_table.columns[i].numbers;
                }
                else if (const std::optional<double> time = toTime(fields[i]))
                {
                    value = *time;
                    ++m_table.columns[i].numbers;
                    ++m_times[i];
                }
            }
            m_table.columns[i].values.push_back(value);
        }
        ++m_table.rows;
    }

    // The table, once every row is in.
    [[nodiscard]] DataTable finish()
    {
        for (std::size_t i = 0; i < m_table.columns.size(); ++i)
        {
            DataTable::Column& column = m_table.columns[i];
            column.isTime             = m_times[i] > 0 && m_times[i] == column.numbers;
            if (column.name.isEmpty())
            {
                column.name = QStringLiteral("Column %1").arg(i + 1);
            }
        }
        return std::move(m_table);
    }

private:
    // Column @p index, added (with the columns before it) if the table doesn't have it yet.
    DataTable::Column& column(std::size_t index)
    {
        while (m_table.columns.size() <= index)
        {
            DataTable::Column& added = m_table.columns.emplace_back();
            added.values.reserve(m_expectedRows);
            added.values.assign(m_table.rows, kGap);
            m_times.push_back(0);
        }
        return m_table.columns[index];
    }

    DataTable                m_table;
    std::size_t              m_expectedRows;
    std::vector<std::size_t> m_times;  // per column: cells that hold a date and time
};

}  // namespace

bool DataTable::isEmpty() const noexcept
{
    return rows == 0 || std::ranges::none_of(columns, &Column::isNumeric);
}

DataTable parseDelimitedText(QByteArrayView text)
{
    constexpr QByteArrayView kByteOrderMark("\xEF\xBB\xBF");
    if (text.startsWith(kByteOrderMark))
    {
        text = text.sliced(kByteOrderMark.size());
    }
    Fields         sample;
    QByteArrayView line;
    for (LineReader reader(text); sample.size() < kSampleLines && reader.next(line);)
    {
        sample.push_back(line);
    }

    const char   separator = separatorOf(sample);
    TableBuilder builder(separator, static_cast<std::size_t>(text.count('\n')) + 1);
    Fields       fields;
    bool         first = true;
    for (LineReader reader(text); reader.next(line);)
    {
        split(line, separator, fields);
        if (first && areNames(fields, separator != ','))
        {
            builder.setNames(fields);
        }
        else
        {
            builder.addRow(fields);
        }
        first = false;
    }
    return builder.finish();
}

}  // namespace rocketplot::demo
