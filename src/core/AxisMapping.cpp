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

}  // namespace

AxisMapping::AxisMapping(Range range, double pixelStart, double pixelEnd) noexcept
  : m_range(range), m_pixelStart(pixelStart), m_pixelEnd(pixelEnd)
{
    const double span = range.span();
    m_pixelsPerValue  = span > 0.0 ? (pixelEnd - pixelStart) / span : 0.0;
    if (m_pixelsPerValue == 0.0)
    {
        // A degenerate range or pixel interval: map everything to pixelStart rather than divide by
        // zero.
        m_pixelsPerValue = std::numeric_limits<double>::min();
    }
}

double AxisMapping::pixelLength() const noexcept
{
    return std::abs(m_pixelEnd - m_pixelStart);
}

bool isUsableRange(Range range) noexcept
{
    if (!range.isValid())
    {
        return false;
    }
    const double span      = range.span();
    const double magnitude = std::max(std::abs(range.min), std::abs(range.max));
    return span <= kMaxSpan && span > magnitude * kMinRelativeSpan &&
           span > std::numeric_limits<double>::min();
}

Range pannedRange(const AxisMapping& mapping, double pixelDelta) noexcept
{
    const double grabbed = mapping.toValue(mapping.pixelStart());
    const double now     = mapping.toValue(mapping.pixelStart() - pixelDelta);
    return mapping.range().shifted(now - grabbed);
}

Range zoomedRange(const AxisMapping& mapping, double pixel, double factor) noexcept
{
    return mapping.range().zoomedAbout(mapping.toValue(pixel), factor);
}

}  // namespace rocketplot::core
