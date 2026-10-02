#pragma once

#include <cmath>
#include <cstdint>

#include "rocketplot/Range.h"

namespace rocketplot::core
{

/// How data values are spaced along an axis.
enum class Scale : std::uint8_t
{
    LINEAR,
    LOG,  ///< log10: each power of ten takes the same length; values <= 0 can't be shown
};

/// Maps an axis's data range onto a pixel interval: range.min lands on pixelStart and range.max on
/// pixelEnd, linearly in the scale's transformed space (log10 for Scale::LOG). pixelEnd may be
/// smaller than pixelStart (a y axis, whose values grow upward). On a log axis, values <= 0 map to
/// NaN or -infinity, which drawing treats as gaps.
class AxisMapping
{
public:
    AxisMapping() = default;
    AxisMapping(Range range, double pixelStart, double pixelEnd,
                Scale scale = Scale::LINEAR) noexcept;

    [[nodiscard]] double toPixel(double value) const noexcept
    {
        return m_pixelStart + ((forward(value) - m_start) * m_pixelsPerUnit);
    }
    [[nodiscard]] double toValue(double pixel) const noexcept
    {
        return inverse(m_start + ((pixel - m_pixelStart) / m_pixelsPerUnit));
    }
    /// The scale's transform (log10 for Scale::LOG) and its inverse.
    [[nodiscard]] double forward(double value) const noexcept
    {
        return m_scale == Scale::LOG ? std::log10(value) : value;
    }
    [[nodiscard]] double inverse(double transformed) const noexcept
    {
        return m_scale == Scale::LOG ? std::pow(10.0, transformed) : transformed;
    }

    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] Scale  scale() const noexcept { return m_scale; }
    [[nodiscard]] double pixelStart() const noexcept { return m_pixelStart; }
    [[nodiscard]] double pixelEnd() const noexcept { return m_pixelEnd; }
    /// Length of the pixel interval, always >= 0.
    [[nodiscard]] double pixelLength() const noexcept;

private:
    Range  m_range{.min = 0.0, .max = 1.0};
    Scale  m_scale         = Scale::LINEAR;
    double m_pixelStart    = 0.0;
    double m_pixelEnd      = 1.0;
    double m_start         = 0.0;  // forward(range.min)
    double m_pixelsPerUnit = 1.0;  // pixels per transformed unit
};

/// Whether an axis can show @p range: finite, increasing, positive on a log scale, and with a span
/// that double arithmetic still resolves into distinct pixels (pan/zoom must not go past this).
[[nodiscard]] bool isUsableRange(Range range, Scale scale = Scale::LINEAR) noexcept;

/// The range after dragging the content by @p pixelDelta (from where it was grabbed to where it is
/// now): the value under the grab point follows the pointer.
[[nodiscard]] Range pannedRange(const AxisMapping& mapping, double pixelDelta) noexcept;

/// The range after zooming by @p factor (< 1 zooms in) about the value under @p pixel, which stays
/// put. On a log scale the zoom is multiplicative.
[[nodiscard]] Range zoomedRange(const AxisMapping& mapping, double pixel, double factor) noexcept;

/// The range in which the value under @p fromPixel appears at @p toPixel, with the content
/// magnified @p scale times about it (> 1 zooms in): a two-finger pinch, which moves and zooms at
/// once. With scale 1 it is a pan, with equal pixels a zoom. Multiplicative on a log scale.
[[nodiscard]] Range transformedRange(const AxisMapping& mapping, double fromPixel, double toPixel,
                                     double scale) noexcept;

/// The values between two pixels, in increasing order (a zoom box's edges).
[[nodiscard]] Range rangeBetween(const AxisMapping& mapping, double pixelA, double pixelB) noexcept;

}  // namespace rocketplot::core
