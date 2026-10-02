#include "core/PolylineClipper.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "core/Decimator.h"

namespace rocketplot::core
{

bool clipSegment(PixelPoint& a, PixelPoint& b, PixelBox box) noexcept
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    // For each edge: p < 0 means the segment enters through it, p > 0 that it leaves through it.
    const std::array<double, 4> p{-dx, dx, -dy, dy};
    const std::array<double, 4> q{a.x - box.left, box.right - a.x, a.y - box.top, box.bottom - a.y};
    double                      enter = 0.0;
    double                      leave = 1.0;
    for (std::size_t edge = 0; edge < p.size(); ++edge)
    {
        if (p.at(edge) == 0.0)
        {
            if (q.at(edge) < 0.0)
            {
                return false;  // parallel to this edge and outside it
            }
            continue;
        }
        const double t = q.at(edge) / p.at(edge);
        if (p.at(edge) < 0.0)
        {
            enter = std::max(enter, t);
        }
        else
        {
            leave = std::min(leave, t);
        }
    }
    if (enter > leave)
    {
        return false;
    }
    const PixelPoint start = a;
    if (enter > 0.0)
    {
        a = {.x = start.x + (enter * dx), .y = start.y + (enter * dy)};
    }
    if (leave < 1.0)
    {
        b = {.x = start.x + (leave * dx), .y = start.y + (leave * dy)};
    }
    return true;
}

void clipPolyline(const Polyline& in, PixelBox box, Polyline& out)
{
    out.clear();
    for (std::size_t r = 0; r < in.runCount(); ++r)
    {
        const auto run = in.run(r);
        if (run.size() == 1)
        {
            if (box.contains(run.front()))
            {
                out.add(run.front());
                out.endRun();
            }
            continue;
        }
        bool open = false;  // whether out's current run ends at run[i - 1]
        for (std::size_t i = 1; i < run.size(); ++i)
        {
            PixelPoint a = run[i - 1];
            PixelPoint b = run[i];
            if (open && box.contains(b))
            {
                out.add(b);  // the common case: the run stays inside
                continue;
            }
            const PixelPoint originalA = a;
            const PixelPoint originalB = b;
            if (!clipSegment(a, b, box))
            {
                out.endRun();
                open = false;
                continue;
            }
            if (!open || a != originalA)
            {
                out.endRun();
                out.add(a);
            }
            out.add(b);
            open = b == originalB;
            if (!open)
            {
                out.endRun();  // the segment left the box
            }
        }
        out.endRun();
    }
}

}  // namespace rocketplot::core
