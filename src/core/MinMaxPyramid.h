#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace rocketplot::core
{

inline constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

/// Summary of a run of y values: extremes with where they occur, the first and last finite value,
/// and whether any value was NaN or infinite (a gap in the line).
struct MinMax
{
    double      min          = std::numeric_limits<double>::infinity();
    double      max          = -std::numeric_limits<double>::infinity();
    std::size_t argMin       = kNoIndex;
    std::size_t argMax       = kNoIndex;
    std::size_t firstFinite  = kNoIndex;
    std::size_t lastFinite   = kNoIndex;
    bool        hasNonFinite = false;

    [[nodiscard]] bool hasFinite() const noexcept { return firstFinite != kNoIndex; }

    /// Adds the value at @p index, which comes after every index already added.
    void add(std::size_t index, double value) noexcept;
    /// Adds a summary of values that all come after the ones already summarized.
    void merge(const MinMax& later) noexcept;
};

/// Precomputed MinMax of fixed blocks of y values at several levels (64 values, 64², 64³, ...), so
/// the summary of any index range costs O(64 × levels) instead of O(range). This is what lets a
/// pixel column covering a million samples be drawn as fast as one covering ten. Appending only
/// computes the new blocks.
class MinMaxPyramid
{
public:
    static constexpr std::size_t kBranching = 64;

    /// Rebuilds every level from @p y.
    void build(std::span<const double> y);
    /// Updates for values appended to the data: @p y is all of it, the first @p oldSize values
    /// unchanged.
    void extend(std::span<const double> y, std::size_t oldSize);
    void clear() noexcept { m_levels.clear(); }

    /// The summary of y[first, last). @p y must be the data the pyramid was built from.
    [[nodiscard]] MinMax query(std::span<const double> y, std::size_t first,
                               std::size_t last) const;

    [[nodiscard]] std::size_t levelCount() const noexcept { return m_levels.size(); }

private:
    // Level k holds the complete blocks of kBranching^(k+1) values; a partial block at the end is
    // left out, and its values are read directly by query().
    std::vector<std::vector<MinMax>> m_levels;
};

}  // namespace rocketplot::core
