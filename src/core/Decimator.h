#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/AxisMapping.h"

namespace rocketplot::core
{

class SeriesData;

/// A point in pixel coordinates.
struct PixelPoint
{
    double x = 0.0;
    double y = 0.0;

    friend constexpr bool operator==(const PixelPoint&, const PixelPoint&) = default;
};

/// An axis-aligned pixel rectangle (left <= right, top <= bottom).
struct PixelBox
{
    double left   = 0.0;
    double top    = 0.0;
    double right  = 0.0;
    double bottom = 0.0;

    [[nodiscard]] bool contains(PixelPoint p) const noexcept
    {
        return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
    }
};

/// One or more connected runs of pixel points, stored back to back. A gap in the data ends a run.
class Polyline
{
public:
    void clear() noexcept;
    /// Adds a point to the current run.
    void add(PixelPoint point) { m_points.push_back(point); }
    /// Ends the current run; the next add() starts a new one. Does nothing if the current run is
    /// empty.
    void endRun();

    [[nodiscard]] std::size_t runCount() const noexcept;
    /// The points of run @p index, after endRun() or for the still-open last run.
    [[nodiscard]] std::span<const PixelPoint> run(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t pointCount() const noexcept { return m_points.size(); }

private:
    std::vector<PixelPoint>  m_points;
    std::vector<std::size_t> m_runEnds;  // one past the last point of each ended run
};

/// How a line was reduced to what is drawn.
enum class DecimationMode : std::uint8_t
{
    RAW,         ///< Every visible point (few enough per pixel column)
    MIN_MAX,     ///< Sorted x: first, min, max and last point of each pixel column
    PIXEL_SKIP,  ///< Unsorted x: every point, minus repeats of the previous point's pixel
};

struct DecimationResult
{
    DecimationMode mode       = DecimationMode::RAW;
    std::size_t visiblePoints = 0;  ///< Points the decimation looked at (the visible index range)
};

/// Fills @p out with the pixel polyline for @p data seen through the @p x and @p y mappings. @p
/// columnWidth is the pixel width of one decimation column: 1 / devicePixelRatio gives one column
/// per device pixel.
///
/// Sorted series only visit the visible index range (plus one point either side, so the line runs
/// to the edges). When there are more than a few points per column, each column becomes its first,
/// min, max and last point in index order, which draws the same pixels as the full data. NaN and
/// infinite values break the line.
DecimationResult decimateLine(const SeriesData& data, const AxisMapping& x, const AxisMapping& y,
                              double columnWidth, Polyline& out);

/// Fills @p out with the pixel position of every finite point of @p data inside @p box, with at
/// most one point per @p cellSize × @p cellSize cell (markers drawn there would look the same).
/// Sorted series only visit the x range of the box. Returns the number of points visited.
std::size_t decimateScatter(const SeriesData& data, const AxisMapping& x, const AxisMapping& y,
                            PixelBox box, double cellSize, std::vector<PixelPoint>& out);

}  // namespace rocketplot::core
