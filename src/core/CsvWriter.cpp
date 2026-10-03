#include "core/CsvWriter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/ErrorData.h"
#include "core/SeriesData.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// Text is handed on in pieces of about this size.
constexpr std::size_t kChunkSize = std::size_t{64} * 1024;

// Walks the points of one series whose x is in a range, in index order. A series sorted by x only
// visits those; an unsorted one looks at every point.
class Cursor
{
public:
    Cursor(const SeriesData& data, Range xRange)
      : m_data(&data), m_range(xRange), m_end(data.size())
    {
        if (data.isSortedByX())
        {
            m_index = std::min(data.lowerBound(xRange.min), m_end);
            m_end   = std::clamp(data.upperBound(xRange.max), m_index, m_end);
        }
        skipHidden();
    }

    [[nodiscard]] bool        atEnd() const noexcept { return m_index >= m_end; }
    [[nodiscard]] std::size_t index() const noexcept { return m_index; }

    void advance()
    {
        ++m_index;
        skipHidden();
    }

private:
    void skipHidden()
    {
        while (m_index < m_end && !m_range.contains(m_data->x(m_index)))
        {
            ++m_index;
        }
    }

    const SeriesData* m_data;
    Range             m_range;
    std::size_t       m_index = 0;
    std::size_t       m_end;
};

// Whether two series have the same x values in the range, point for point.
bool haveSameX(const SeriesData& a, const SeriesData& b, Range xRange)
{
    Cursor first(a, xRange);
    Cursor second(b, xRange);
    while (!first.atEnd() && !second.atEnd())
    {
        if (a.x(first.index()) != b.x(second.index()))
        {
            return false;
        }
        first.advance();
        second.advance();
    }
    return first.atEnd() && second.atEnd();
}

// The series (as indices) in groups that have the same x values, each group in the order given
// and the groups in the order of their first series.
std::vector<std::vector<std::size_t>> groupByX(std::span<const CsvSeries> series, Range xRange)
{
    std::vector<std::vector<std::size_t>> groups;
    for (std::size_t s = 0; s < series.size(); ++s)
    {
        const auto group = std::ranges::find_if(groups, [&](const std::vector<std::size_t>& other) {
            return haveSameX(*series[other.front()].data, *series[s].data, xRange);
        });
        if (group != groups.end())
        {
            group->push_back(s);
        }
        else
        {
            groups.push_back({s});
        }
    }
    return groups;
}

// Builds the text a row at a time and hands it on in pieces.
class Output
{
public:
    Output(char separator, const std::function<void(std::string_view)>& write)
      : m_separator(separator), m_write(&write)
    {
        m_text.reserve(kChunkSize + 256);
    }

    void name(std::string_view text)
    {
        startCell();
        const bool quote =
            text.find_first_of(std::string{m_separator, '"', '\n', '\r'}) != std::string_view::npos;
        if (!quote)
        {
            m_text += text;
            return;
        }
        m_text += '"';
        for (const char character : text)
        {
            m_text += character;
            if (character == '"')
            {
                m_text += '"';  // a quote inside quotes is doubled
            }
        }
        m_text += '"';
    }

    void number(double value)
    {
        startCell();
        if (!std::isnan(value))
        {
            std::format_to(std::back_inserter(m_text), "{}", value);
        }
    }

    void empty() { startCell(); }

    void endRow()
    {
        m_text += '\n';
        m_rowStarted = false;
        if (m_text.size() >= kChunkSize)
        {
            flush();
        }
    }

    void flush()
    {
        if (!m_text.empty())
        {
            (*m_write)(m_text);
            m_text.clear();
        }
    }

private:
    void startCell()
    {
        if (m_rowStarted)
        {
            m_text += m_separator;
        }
        m_rowStarted = true;
    }

    char                                         m_separator;
    const std::function<void(std::string_view)>* m_write;
    std::string                                  m_text;
    bool                                         m_rowStarted = false;
};

// The columns of one series after its x: y, then the ends of its error bars.
void writeHeader(const CsvSeries& series, Output& out)
{
    out.name(series.name);
    if (series.errors != nullptr && series.errors->hasX())
    {
        out.name(series.name + " x (low)");
        out.name(series.name + " x (high)");
    }
    if (series.errors != nullptr && series.errors->hasY())
    {
        out.name(series.name + " (low)");
        out.name(series.name + " (high)");
    }
}

// The ends of an error bar; empty cells for a point added after the errors were set, or for no
// point at all.
void writeEnds(std::span<const double> low, std::span<const double> high, std::size_t index,
               Output& out)
{
    if (index < low.size())
    {
        out.number(low[index]);
        out.number(high[index]);
        return;
    }
    out.empty();
    out.empty();
}

// The cells of one series in a row: for the point at @p index, or empty ones past its last point.
void writeCells(const CsvSeries& series, const Cursor& cursor, Output& out)
{
    const std::size_t index = cursor.atEnd() ? series.data->size() : cursor.index();
    if (cursor.atEnd())
    {
        out.empty();
    }
    else
    {
        out.number(series.data->y(index));
    }
    if (series.errors != nullptr && series.errors->hasX())
    {
        writeEnds(series.errors->xLow(), series.errors->xHigh(), index, out);
    }
    if (series.errors != nullptr && series.errors->hasY())
    {
        writeEnds(series.errors->yLow(), series.errors->yHigh(), index, out);
    }
}

}  // namespace

void writeCsv(const CsvTable& table, const std::function<void(std::string_view)>& write)
{
    const std::span<const CsvSeries> series = table.series;
    if (series.empty())
    {
        return;
    }
    const std::vector<std::vector<std::size_t>> groups = groupByX(series, table.xRange);
    Output                                      out(table.separator, write);
    for (const std::vector<std::size_t>& group : groups)
    {
        // The first x column serves the series up to the next one; later ones say whose they are.
        const bool first = &group == &groups.front();
        out.name(first ? table.xName : table.xName + " (" + series[group.front()].name + ")");
        for (const std::size_t s : group)
        {
            writeHeader(series[s], out);
        }
    }
    out.endRow();

    std::vector<Cursor> cursors;
    cursors.reserve(series.size());
    for (const CsvSeries& one : series)
    {
        cursors.emplace_back(*one.data, table.xRange);
    }
    const auto anyLeft = [&] {
        return std::ranges::any_of(cursors, [](const Cursor& cursor) { return !cursor.atEnd(); });
    };
    while (anyLeft())
    {
        for (const std::vector<std::size_t>& group : groups)
        {
            // Every series of a group is at the same x: the first one's is written.
            const Cursor& leader = cursors[group.front()];
            if (leader.atEnd())
            {
                out.empty();
            }
            else
            {
                out.number(series[group.front()].data->x(leader.index()));
            }
            for (const std::size_t s : group)
            {
                writeCells(series[s], cursors[s], out);
                if (!cursors[s].atEnd())
                {
                    cursors[s].advance();
                }
            }
        }
        out.endRow();
    }
    out.flush();
}

}  // namespace rocketplot::core
