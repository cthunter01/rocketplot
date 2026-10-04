#include "core/MinMaxPyramid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace rocketplot::core
{

void MinMax::add(std::size_t index, double value) noexcept
{
    if (!std::isfinite(value))
    {
        hasNonFinite = true;
        return;
    }
    if (firstFinite == kNoIndex)
    {
        firstFinite = index;
    }
    lastFinite = index;
    if (value > 0.0 && value < minPositive)
    {
        minPositive = value;
    }
    if (value < min)
    {
        min    = value;
        argMin = index;
    }
    if (value > max)
    {
        max    = value;
        argMax = index;
    }
}

void MinMax::merge(const MinMax& later) noexcept
{
    hasNonFinite = hasNonFinite || later.hasNonFinite;
    if (!later.hasFinite())
    {
        return;
    }
    if (firstFinite == kNoIndex)
    {
        firstFinite = later.firstFinite;
    }
    lastFinite  = later.lastFinite;
    minPositive = std::min(minPositive, later.minPositive);
    if (later.min < min)
    {
        min    = later.min;
        argMin = later.argMin;
    }
    if (later.max > max)
    {
        max    = later.max;
        argMax = later.argMax;
    }
}

void MinMaxPyramid::build(std::span<const double> y)
{
    m_levels.clear();
    extend(y, 0);
}

void MinMaxPyramid::extend(std::span<const double> y, std::size_t oldSize)
{
    std::size_t blockSize = kBranching;
    for (std::size_t level = 0; y.size() / blockSize > 0; ++level, blockSize *= kBranching)
    {
        if (level == m_levels.size())
        {
            m_levels.emplace_back();
        }
        auto&             blocks   = m_levels[level];
        const std::size_t complete = y.size() / blockSize;
        // Blocks that were complete before the append are unchanged.
        blocks.resize(std::min(blocks.size(), oldSize / blockSize));
        // Room for the new blocks at once, but growing at least twofold: a reserve() of just
        // what is needed would copy every block again each time an append completes one.
        if (blocks.capacity() < complete)
        {
            blocks.reserve(std::max(complete, blocks.capacity() * 2));
        }
        for (std::size_t block = blocks.size(); block < complete; ++block)
        {
            MinMax summary;
            if (level == 0)
            {
                const std::size_t first = block * blockSize;
                for (std::size_t i = first; i < first + blockSize; ++i)
                {
                    summary.add(i, y[i]);
                }
            }
            else
            {
                const auto& children = m_levels[level - 1];
                for (std::size_t child = block * kBranching; child < (block + 1) * kBranching;
                     ++child)
                {
                    summary.merge(children[child]);
                }
            }
            blocks.push_back(summary);
        }
    }
}

MinMax MinMaxPyramid::query(std::span<const double> y, std::size_t first, std::size_t last) const
{
    MinMax      result;
    std::size_t i = first;
    while (i < last)
    {
        // The largest complete block that starts at i and ends by last; otherwise one raw value.
        bool usedBlock = false;
        for (std::size_t level = m_levels.size(); level-- > 0;)
        {
            std::size_t blockSize = kBranching;
            for (std::size_t k = 0; k < level; ++k)
            {
                blockSize *= kBranching;
            }
            const std::size_t block = i / blockSize;
            if (i % blockSize == 0 && i + blockSize <= last && block < m_levels[level].size())
            {
                result.merge(m_levels[level][block]);
                i += blockSize;
                usedBlock = true;
                break;
            }
        }
        if (!usedBlock)
        {
            result.add(i, y[i]);
            ++i;
        }
    }
    return result;
}

}  // namespace rocketplot::core
