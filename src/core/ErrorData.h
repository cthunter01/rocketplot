#pragma once

#include <span>
#include <vector>

#include "core/MinMaxPyramid.h"
#include "rocketplot/Range.h"

namespace rocketplot::core
{

class SeriesData;

/// How far off each point of a series may be: the ends of its error bars along x and along y, kept
/// as data values (y - minus and y + plus), with what drawing and autoscale need derived from them.
///
/// The errors belong to the points the series had when they were set: a series drops them when its
/// data is replaced, and points appended later have none.
class ErrorData
{
public:
    /// Point i's bar runs from x - |minus[i]| to x + |plus[i]|. An error that is NaN or infinite
    /// counts as 0, and a point that is a gap has no bar.
    /// @throws std::invalid_argument unless minus and plus each have data.size() values.
    void setX(const SeriesData& data, std::span<const double> minus, std::span<const double> plus);
    /// The same along y.
    void setY(const SeriesData& data, std::span<const double> minus, std::span<const double> plus);
    void clear();

    [[nodiscard]] bool hasX() const noexcept { return !m_x.low.empty(); }
    [[nodiscard]] bool hasY() const noexcept { return !m_y.low.empty(); }
    [[nodiscard]] bool empty() const noexcept { return !hasX() && !hasY(); }

    /// The ends of the bars, one per point they were set for (NaN where the point is a gap); empty
    /// without errors along that axis. low <= the point's value <= high.
    [[nodiscard]] std::span<const double> xLow() const noexcept { return m_x.low; }
    [[nodiscard]] std::span<const double> xHigh() const noexcept { return m_x.high; }
    [[nodiscard]] std::span<const double> yLow() const noexcept { return m_y.low; }
    [[nodiscard]] std::span<const double> yHigh() const noexcept { return m_y.high; }

    /// Bounds of the bars' ends; Range::empty() without errors along that axis.
    [[nodiscard]] Range xBounds() const noexcept { return m_x.bounds; }
    [[nodiscard]] Range yBounds() const noexcept { return m_y.bounds; }
    /// The same, counting only the positive ends of points whose value is positive: what a log
    /// axis can show.
    [[nodiscard]] Range xPositiveBounds() const noexcept { return m_x.positive; }
    [[nodiscard]] Range yPositiveBounds() const noexcept { return m_y.positive; }
    /// Bounds of the y bars' ends of the points of @p data whose x is in @p xRange (only positive
    /// ends when @p positiveOnly). Fast for sorted series; unsorted ones visit every point.
    [[nodiscard]] Range yBoundsWithin(const SeriesData& data, Range xRange,
                                      bool positiveOnly) const;

    /// The longest x errors below and above their points: how far beside the view a point can be
    /// and still reach into it.
    [[nodiscard]] double xReachBelow() const noexcept { return m_x.reachBelow; }
    [[nodiscard]] double xReachAbove() const noexcept { return m_x.reachAbove; }

    /// Pyramids of yLow() and yHigh(): the extent of an error band over any run of points.
    [[nodiscard]] const MinMaxPyramid& yLowPyramid() const noexcept { return m_yLowPyramid; }
    [[nodiscard]] const MinMaxPyramid& yHighPyramid() const noexcept { return m_yHighPyramid; }

private:
    struct Ends
    {
        std::vector<double> low;
        std::vector<double> high;
        Range               bounds     = Range::empty();
        Range               positive   = Range::empty();
        double              reachBelow = 0.0;
        double              reachAbove = 0.0;
    };

    [[nodiscard]] static Ends endsOf(const SeriesData& data, bool alongX,
                                     std::span<const double> minus, std::span<const double> plus);

    Ends          m_x;
    Ends          m_y;
    MinMaxPyramid m_yLowPyramid;
    MinMaxPyramid m_yHighPyramid;
};

}  // namespace rocketplot::core
