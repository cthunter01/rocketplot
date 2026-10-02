#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/MinMaxPyramid.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"

namespace rocketplot::core
{

/// The points of one series and everything derived from them that drawing needs: whether x is
/// sorted, the bounds of the finite points, and the min/max pyramid of y.
///
/// x is either an array or implicit (UniformX). The data is either owned, or a view of the caller's
/// memory that must stay alive and unchanged until replaced; after changing viewed memory in place,
/// call refresh().
///
/// x is sorted when it never decreases and has no NaN; sorted series are drawn by binary search and
/// per-pixel min/max, unsorted ones by visiting every point. A point whose x or y is NaN or
/// infinite is a gap.
class SeriesData
{
public:
    /// @throws std::invalid_argument if x and y differ in size.
    void setOwned(std::vector<double> x, std::vector<double> y);
    void setOwned(UniformX x, std::vector<double> y);
    /// @throws std::invalid_argument if x and y differ in size.
    void setView(std::span<const double> x, std::span<const double> y);
    void setView(UniformX x, std::span<const double> y);
    /// Recomputes everything derived from the data (after viewed memory changed in place).
    void refresh();
    /// Empty, owned, explicit x.
    void clear();

    /// Adds points to an owned series with an x array.
    /// @throws std::invalid_argument if x and y differ in size; std::logic_error for a view or a
    /// UniformX series.
    void append(std::span<const double> x, std::span<const double> y);
    /// Adds samples to an owned UniformX series.
    /// @throws std::logic_error for a view or a series with an x array.
    void append(std::span<const double> y);

    [[nodiscard]] std::size_t size() const noexcept { return m_y.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_y.empty(); }
    [[nodiscard]] double      x(std::size_t index) const noexcept
    {
        return m_isUniform ? m_uniform.start + (static_cast<double>(index) * m_uniform.step)
                           : m_x[index];
    }
    [[nodiscard]] double y(std::size_t index) const noexcept { return m_y[index]; }
    /// The y values (always an array).
    [[nodiscard]] std::span<const double> ys() const noexcept { return m_y; }
    /// The x values; empty for a UniformX series.
    [[nodiscard]] std::span<const double> xs() const noexcept { return m_x; }

    [[nodiscard]] bool     isUniform() const noexcept { return m_isUniform; }
    [[nodiscard]] UniformX uniform() const noexcept { return m_uniform; }
    [[nodiscard]] bool     isView() const noexcept { return m_isView; }
    [[nodiscard]] bool     isSortedByX() const noexcept { return m_sorted; }

    /// Bounds of the points whose x and y are both finite; Range::empty() when there are none.
    [[nodiscard]] Range xBounds() const noexcept { return m_xBounds; }
    [[nodiscard]] Range yBounds() const noexcept { return m_yBounds; }
    /// The same, counting only points whose x (or y) is positive: what a log axis can show.
    [[nodiscard]] Range xPositiveBounds() const noexcept { return m_xPositive; }
    [[nodiscard]] Range yPositiveBounds() const noexcept { return m_yPositive; }
    /// Bounds of the finite y values of the points whose x is in @p xRange (only positive ones
    /// when @p positiveOnly). Range::empty() when there are none. Fast for sorted series (it reads
    /// the pyramid); unsorted ones visit every point.
    [[nodiscard]] Range yBoundsWithin(Range xRange, bool positiveOnly) const;

    [[nodiscard]] const MinMaxPyramid& pyramid() const noexcept { return m_pyramid; }

    /// First index whose x is >= @p value (size() if none). Only meaningful when isSortedByX().
    [[nodiscard]] std::size_t lowerBound(double value) const;
    /// First index whose x is > @p value (size() if none). Only meaningful when isSortedByX().
    [[nodiscard]] std::size_t upperBound(double value) const;
    /// The point whose x is nearest @p value (on a tie, the earlier one), when the series is sorted
    /// by x and @p value lies within its x range; nothing otherwise.
    [[nodiscard]] std::optional<std::size_t> nearestIndex(double value) const;

private:
    void resetDerived();
    // Updates sortedness, bounds and the pyramid for the points from index first on.
    void updateDerived(std::size_t first);

    std::vector<double>     m_xOwned;
    std::vector<double>     m_yOwned;
    std::span<const double> m_x;  // the x array, owned or viewed (empty when uniform)
    std::span<const double> m_y;  // the y array, owned or viewed
    UniformX                m_uniform;
    bool                    m_isUniform = false;
    bool                    m_isView    = false;
    bool                    m_sorted    = true;
    Range                   m_xBounds   = Range::empty();
    Range                   m_yBounds   = Range::empty();
    Range                   m_xPositive = Range::empty();
    Range                   m_yPositive = Range::empty();
    MinMaxPyramid           m_pyramid;
};

}  // namespace rocketplot::core
