#include "core/PolylineClipper.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/Decimator.h"

namespace rocketplot::core
{

namespace
{

constexpr std::size_t kNoEdge = 4;

// The point at parameter t along start + t * (dx, dy), on @p edge of the box (0 left, 1 right,
// 2 top, 3 bottom). Its coordinate across that edge is the edge's own, exactly: computed, it could
// miss by an ulp, more so where the compiler fuses the multiply and add.
PixelPoint pointOnEdge(PixelPoint start, double dx, double dy, double t, std::size_t edge,
                       PixelBox box)
{
    PixelPoint point{.x = start.x + (t * dx), .y = start.y + (t * dy)};
    switch (edge)
    {
        case 0:
            point.x = box.left;
            break;
        case 1:
            point.x = box.right;
            break;
        case 2:
            point.y = box.top;
            break;
        case 3:
            point.y = box.bottom;
            break;
        default:
            break;
    }
    return point;
}

double distance(PixelPoint a, PixelPoint b)
{
    return std::hypot(b.x - a.x, b.y - a.y);
}

}  // namespace

bool clipSegment(PixelPoint& a, PixelPoint& b, PixelBox box) noexcept
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    // For each edge: p < 0 means the segment enters through it, p > 0 that it leaves through it.
    const std::array<double, 4> p{-dx, dx, -dy, dy};
    const std::array<double, 4> q{a.x - box.left, box.right - a.x, a.y - box.top, box.bottom - a.y};
    double                      enter     = 0.0;
    double                      leave     = 1.0;
    std::size_t                 enterEdge = kNoEdge;
    std::size_t                 leaveEdge = kNoEdge;
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
        if (p.at(edge) < 0.0 && t > enter)
        {
            enter     = t;
            enterEdge = edge;
        }
        else if (p.at(edge) > 0.0 && t < leave)
        {
            leave     = t;
            leaveEdge = edge;
        }
    }
    if (enter > leave)
    {
        return false;
    }
    const PixelPoint start = a;
    if (enter > 0.0)
    {
        a = pointOnEdge(start, dx, dy, enter, enterEdge, box);
    }
    if (leave < 1.0)
    {
        b = pointOnEdge(start, dx, dy, leave, leaveEdge, box);
    }
    return true;
}

namespace
{

// Clips one run of two or more points (see clipPolyline()); with @p runOffsets, measures how far
// along it each run of @p out starts.
void clipRun(std::span<const PixelPoint> run, PixelBox box, Polyline& out,
             std::vector<double>* runOffsets)
{
    const bool measure   = runOffsets != nullptr;
    bool       open      = false;  // whether out's current run ends at run[i - 1]
    double     travelled = 0.0;    // along the run, to run[i - 1]
    for (std::size_t i = 1; i < run.size(); ++i)
    {
        PixelPoint   a      = run[i - 1];
        PixelPoint   b      = run[i];
        const double length = measure ? distance(a, b) : 0.0;
        if (open && box.contains(b))
        {
            out.add(b);  // the common case: the run stays inside
            travelled += length;
            continue;
        }
        const PixelPoint originalA = a;
        const PixelPoint originalB = b;
        if (!clipSegment(a, b, box))
        {
            out.endRun();
            open = false;
            travelled += length;
            continue;
        }
        if (!open || a != originalA)
        {
            out.endRun();
            out.add(a);
            if (measure)
            {
                runOffsets->push_back(travelled + distance(originalA, a));
            }
        }
        out.add(b);
        travelled += length;
        open = b == originalB;
        if (!open)
        {
            out.endRun();  // the segment left the box
        }
    }
    out.endRun();
}

}  // namespace

void clipPolyline(const Polyline& in, PixelBox box, Polyline& out, std::vector<double>* runOffsets)
{
    out.clear();
    if (runOffsets != nullptr)
    {
        runOffsets->clear();
    }
    for (std::size_t r = 0; r < in.runCount(); ++r)
    {
        const auto run = in.run(r);
        if (run.size() > 1)
        {
            clipRun(run, box, out, runOffsets);
        }
        else if (box.contains(run.front()))
        {
            out.add(run.front());
            out.endRun();
            if (runOffsets != nullptr)
            {
                runOffsets->push_back(0.0);
            }
        }
    }
}

}  // namespace rocketplot::core
