#include "core/AxisMapping.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

namespace
{

// Spans beyond this overflow pixel arithmetic. Below kMinRelativeSpan of the values' magnitude, a
// span is only a few hundred doubles wide (one ulp is 2.2e-16 relative) and lines would step
// visibly. Epoch seconds can still be zoomed to a millisecond (6e-13 relative).
constexpr double kMaxSpan         = 1e300;
constexpr double kMinRelativeSpan = 1e-13;
// Log ranges: at most this many decades (doubles cover about 600).
constexpr double kMaxDecades = 600.0;

}  // namespace

AxisMapping::AxisMapping(Range range, double pixelStart, double pixelEnd, Scale scale) noexcept
  : m_range(range),
    m_scale(scale),
    m_pixelStart(pixelStart),
    m_pixelEnd(pixelEnd),
    m_start(forward(range.min))
{
    const double span = forward(range.max) - m_start;
    m_pixelsPerUnit   = span > 0.0 ? (pixelEnd - pixelStart) / span : 0.0;
    if (m_pixelsPerUnit == 0.0 || !std::isfinite(m_pixelsPerUnit))
    {
        // A degenerate range or pixel interval: map everything near pixelStart rather than divide
        // by zero.
        m_pixelsPerUnit = std::numeric_limits<double>::min();
    }
    if (!std::isfinite(m_start))
    {
        m_start = 0.0;
    }
}

double AxisMapping::pixelLength() const noexcept
{
    return std::abs(m_pixelEnd - m_pixelStart);
}

bool isUsableRange(Range range, Scale scale) noexcept
{
    if (!range.isValid())
    {
        return false;
    }
    if (scale == Scale::LOG)
    {
        if (!(range.min > 0.0))
        {
            return false;
        }
        const double decades = std::log10(range.max) - std::log10(range.min);
        if (!(decades <= kMaxDecades))
        {
            return false;
        }
    }
    const double span      = range.span();
    const double magnitude = std::max(std::abs(range.min), std::abs(range.max));
    return span <= kMaxSpan && span > magnitude * kMinRelativeSpan &&
           span > std::numeric_limits<double>::min();
}

Range pannedRange(const AxisMapping& mapping, double pixelDelta) noexcept
{
    // In transformed space a pan is a shift (on a log scale: a multiplication).
    const double shift = mapping.forward(mapping.toValue(mapping.pixelStart() - pixelDelta)) -
                         mapping.forward(mapping.toValue(mapping.pixelStart()));
    const Range  range = mapping.range();
    return {
        .min = mapping.inverse(mapping.forward(range.min) + shift),
        .max = mapping.inverse(mapping.forward(range.max) + shift),
    };
}

Range zoomedRange(const AxisMapping& mapping, double pixel, double factor) noexcept
{
    const Range  range  = mapping.range();
    const double anchor = mapping.forward(mapping.toValue(pixel));
    const double start  = mapping.forward(range.min);
    const double end    = mapping.forward(range.max);
    return {
        .min = mapping.inverse(anchor - ((anchor - start) * factor)),
        .max = mapping.inverse(anchor + ((end - anchor) * factor)),
    };
}

Range transformedRange(const AxisMapping& mapping, double fromPixel, double toPixel,
                       double scale) noexcept
{
    const Range  range  = mapping.range();
    const double pixels = mapping.pixelEnd() - mapping.pixelStart();
    // Nothing to do (exactly: rounding would move the range by an ulp).
    if (pixels == 0.0 || !(scale > 0.0) || !std::isfinite(scale) ||
        (scale == 1.0 && fromPixel == toPixel))
    {
        return range;
    }
    // In transformed space: the anchor value sits at toPixel, with scale times fewer units per
    // pixel.
    const double anchor        = mapping.forward(mapping.toValue(fromPixel));
    const double unitsPerPixel = (mapping.forward(range.max) - mapping.forward(range.min)) / pixels;
    const double scaled        = unitsPerPixel / scale;
    return {
        .min = mapping.inverse(anchor + ((mapping.pixelStart() - toPixel) * scaled)),
        .max = mapping.inverse(anchor + ((mapping.pixelEnd() - toPixel) * scaled)),
    };
}

Range rangeBetween(const AxisMapping& mapping, double pixelA, double pixelB) noexcept
{
    const double a = mapping.toValue(pixelA);
    const double b = mapping.toValue(pixelB);
    return {.min = std::min(a, b), .max = std::max(a, b)};
}

}  // namespace rocketplot::core
