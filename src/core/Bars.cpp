#include "core/Bars.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/AxisMapping.h"
#include "core/Decimator.h"
#include "core/NearestPoint.h"

namespace rocketplot::core
{

namespace
{

// How far past the clip box a bar may reach: drawn edges outside it are clipped anyway.
constexpr double kOverreach = 1.0;

double snapped(double pixel, double devicePixelRatio)
{
    return std::round(pixel * devicePixelRatio) / devicePixelRatio;
}

// The pixel a bar's height reaches, or nothing for one that can't be shown.
std::optional<double> heightPixel(const AxisMapping& y, double height)
{
    if (!std::isfinite(height) || (y.scale() == Scale::LOG && !(height > 0.0)))
    {
        return std::nullopt;
    }
    const double pixel = y.toPixel(height);
    return std::isfinite(pixel) ? std::optional(pixel) : std::nullopt;
}

double baselinePixel(const AxisMapping& y, PixelBox clip)
{
    return y.scale() == Scale::LOG ? clip.bottom : y.toPixel(0.0);
}

// The bins [first, last) whose x range may reach into @p clip.
std::pair<std::size_t, std::size_t> visibleBins(std::span<const double> edges, const AxisMapping& x,
                                                PixelBox clip)
{
    const std::size_t bins = edges.size() - 1;
    double            from = x.toValue(clip.left - kOverreach);
    double            to   = x.toValue(clip.right + kOverreach);
    if (from > to)
    {
        std::swap(from, to);
    }
    if (!(from <= to))  // NaN: all of them
    {
        return {0, bins};
    }
    const auto upper = std::ranges::upper_bound(edges, from);
    const auto first = static_cast<std::size_t>(
        std::max<std::ptrdiff_t>(std::distance(edges.begin(), upper) - 1, 0));
    const auto last = std::min(
        static_cast<std::size_t>(std::distance(edges.begin(), std::ranges::lower_bound(edges, to))),
        bins);
    return {std::min(first, last), last};
}

// A run of bars drawn as one: from @p left to @p right, reaching from @p top to @p bottom.
struct Group
{
    std::size_t index  = 0;
    double      left   = 0.0;
    double      right  = 0.0;
    double      top    = 0.0;
    double      bottom = 0.0;
    bool        drawn  = false;  // whether any of its bars can be shown
};

}  // namespace

std::optional<std::size_t> binAt(std::span<const double> edges, double x)
{
    if (edges.size() < 2 || !(x >= edges.front()) || !(x <= edges.back()))  // NaN too
    {
        return std::nullopt;
    }
    const auto above = std::ranges::upper_bound(edges, x);
    const auto bin   = static_cast<std::size_t>(std::distance(edges.begin(), above)) - 1;
    return std::min(bin, edges.size() - 2);  // the last edge belongs to the last bin
}

void barRects(std::span<const double> edges, std::span<const double> heights, const AxisMapping& x,
              const AxisMapping& y, PixelBox clip, BarStyle style, std::vector<BarRect>& out)
{
    out.clear();
    if (edges.size() < 2 || heights.size() + 1 != edges.size())
    {
        return;
    }
    const double dpr      = style.devicePixelRatio > 0.0 ? style.devicePixelRatio : 1.0;
    const double minWidth = 1.0 / dpr;
    const double gap      = std::clamp(style.gap, 0.0, 1.0);
    const double base     = snapped(
        std::clamp(baselinePixel(y, clip), clip.top - kOverreach, clip.bottom + kOverreach), dpr);
    const auto emit = [&](const Group& group) {
        if (group.drawn && group.bottom > group.top)
        {
            out.push_back({.box   = {.left   = group.left,
                                     .top    = group.top,
                                     .right  = group.right,
                                     .bottom = group.bottom},
                           .index = group.index});
        }
    };
    const auto [first, last] = visibleBins(edges, x, clip);
    std::optional<Group> group;
    for (std::size_t i = first; i < last; ++i)
    {
        const double rawLeft  = x.toPixel(edges[i]);
        const double rawRight = x.toPixel(edges[i + 1]);
        if (!std::isfinite(rawLeft) || !std::isfinite(rawRight))
        {
            continue;  // an edge a log axis can't show
        }
        const double inset = gap * (rawRight - rawLeft) / 2.0;
        // Touching neighbours snap the same edge the same way, and so share it.
        const double left  = snapped(rawLeft + inset, dpr);
        const double right = snapped(rawRight - inset, dpr);
        if (!group)
        {
            group = Group{.index = i, .left = left, .right = left, .top = base, .bottom = base};
        }
        group->right = std::max(group->right, right);
        if (const std::optional<double> top = heightPixel(y, heights[i]))
        {
            const double reach =
                snapped(std::clamp(*top, clip.top - kOverreach, clip.bottom + kOverreach), dpr);
            group->top    = std::min(group->top, reach);
            group->bottom = std::max(group->bottom, reach);
            group->drawn  = true;
        }
        if (group->right - group->left >= minWidth - 1e-9)
        {
            emit(*group);
            group.reset();
        }
    }
    if (group && group->drawn)
    {
        // The last run is too narrow by itself: part of the bar before it, if that touches it, or
        // else as wide as a device pixel.
        if (!out.empty() && out.back().box.right == group->left)
        {
            PixelBox& before = out.back().box;
            before.right     = group->right;
            before.top       = std::min(before.top, group->top);
            before.bottom    = std::max(before.bottom, group->bottom);
        }
        else
        {
            group->right = std::max(group->right, group->left + minWidth);
            emit(*group);
        }
    }
}

std::optional<NearestPoint> pickBar(std::span<const double> edges, std::span<const double> heights,
                                    const PickQuery& query)
{
    if (edges.size() < 2 || heights.size() + 1 != edges.size() || query.x == nullptr ||
        query.y == nullptr)
    {
        return std::nullopt;
    }
    const AxisMapping& x    = *query.x;
    const AxisMapping& y    = *query.y;
    const double       base = baselinePixel(y, query.box);
    // The point a bar stands for, and how far the pointer is from its rectangle.
    const auto measure = [&](std::size_t bin) -> std::optional<NearestPoint> {
        const std::optional<double> top   = heightPixel(y, heights[bin]);
        const double                left  = x.toPixel(edges[bin]);
        const double                right = x.toPixel(edges[bin + 1]);
        if (!top || !std::isfinite(left) || !std::isfinite(right) || right < query.box.left ||
            left > query.box.right)
        {
            return std::nullopt;
        }
        const double low  = std::min(*top, base);
        const double high = std::max(*top, base);
        const double dx   = std::max({std::min(left, right) - query.position.x, 0.0,
                                      query.position.x - std::max(left, right)});
        const double dy   = std::max({low - query.position.y, 0.0, query.position.y - high});
        return NearestPoint{.index    = bin,
                            .pixel    = {.x = (left + right) / 2.0, .y = *top},
                            .distance = std::hypot(dx, dy)};
    };
    if (query.tracing)
    {
        const std::optional<std::size_t> bin = binAt(edges, x.toValue(query.position.x));
        return bin ? measure(*bin) : std::nullopt;
    }
    const PixelBox reach{.left   = query.position.x - query.reach,
                         .top    = query.box.top,
                         .right  = query.position.x + query.reach,
                         .bottom = query.box.bottom};
    const auto [first, last] = visibleBins(edges, x, reach);
    std::optional<NearestPoint> nearest;
    for (std::size_t bin = first; bin < last; ++bin)
    {
        const std::optional<NearestPoint> found = measure(bin);
        if (found && found->distance <= query.reach &&
            (!nearest || found->distance < nearest->distance))
        {
            nearest = found;
        }
    }
    return nearest;
}

}  // namespace rocketplot::core
