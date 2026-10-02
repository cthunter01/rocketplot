#pragma once

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// Maps an axis's data range linearly onto a pixel interval: range.min lands on pixelStart and
/// range.max on pixelEnd. pixelEnd may be smaller than pixelStart (a y axis, whose values grow
/// upward).
class AxisMapping
{
public:
    AxisMapping() = default;
    AxisMapping(Range range, double pixelStart, double pixelEnd) noexcept;

    [[nodiscard]] double toPixel(double value) const noexcept
    {
        return m_pixelStart + ((value - m_range.min) * m_pixelsPerValue);
    }
    [[nodiscard]] double toValue(double pixel) const noexcept
    {
        return m_range.min + ((pixel - m_pixelStart) / m_pixelsPerValue);
    }

    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] double pixelStart() const noexcept { return m_pixelStart; }
    [[nodiscard]] double pixelEnd() const noexcept { return m_pixelEnd; }
    /// Length of the pixel interval, always >= 0.
    [[nodiscard]] double pixelLength() const noexcept;

private:
    Range  m_range{.min = 0.0, .max = 1.0};
    double m_pixelStart     = 0.0;
    double m_pixelEnd       = 1.0;
    double m_pixelsPerValue = 1.0;
};

/// Whether an axis can show @p range: finite, increasing, and with a span that double arithmetic
/// still resolves into distinct pixels (pan/zoom must not go past this).
[[nodiscard]] bool isUsableRange(Range range) noexcept;

/// The range after dragging the content by @p pixelDelta (from where it was grabbed to where it is
/// now): the value under the grab point follows the pointer.
[[nodiscard]] Range pannedRange(const AxisMapping& mapping, double pixelDelta) noexcept;

/// The range after zooming by @p factor (< 1 zooms in) about the value under @p pixel, which stays
/// put.
[[nodiscard]] Range zoomedRange(const AxisMapping& mapping, double pixel, double factor) noexcept;

}  // namespace rocketplot::core
