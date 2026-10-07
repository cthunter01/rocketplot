#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "core/AxisMapping.h"
#include "core/Decimator.h"

namespace rocketplot::core
{

class SeriesData;

/// A point of a series found near a position: which one, where it is drawn, and how far away that
/// is (in pixels).
struct NearestPoint
{
    std::size_t index = 0;
    PixelPoint  pixel;
    double      distance = 0.0;
};

/// The point of @p data drawn nearest @p position, seen through the @p x and @p y mappings: what a
/// crosshair that snaps to the data lands on. Only points drawn inside @p box and no more than
/// @p reach pixels from @p position count (an infinite reach: any in the box); nothing if there is
/// none. Of points equally near, it is the first.
///
/// @p sizes are the points' own marker sizes, if they have them: a point whose size isn't positive
/// is hidden, so it is left out (as decimateScatter() does).
///
/// A series sorted by x only visits the points near @p position: the min/max pyramid tells which
/// runs of points are too far away to hold a nearer one, so a pixel column of a million samples
/// costs little more than one of a hundred. An unsorted series visits every point.
[[nodiscard]] std::optional<NearestPoint> nearestPoint(const SeriesData& data, const AxisMapping& x,
                                                       const AxisMapping& y, PixelBox box,
                                                       PixelPoint position, double reach,
                                                       std::span<const double> sizes = {});

/// The point of @p data at the x of @p position: what a crosshair that traces the series shows.
///
/// For a series sorted by x that is, of the points in the pixel column of @p position, the one
/// nearest to it; where the points are farther apart than that, the nearer in x of the two points
/// on either side (the other, if that one can't be shown: a gap, or outside @p box). Nothing
/// before the first point and after the last, or when neither neighbor can be shown.
///
/// A series that isn't sorted by x has no one point at an x: for it, this is nearestPoint() with
/// no limit on the reach.
[[nodiscard]] std::optional<NearestPoint> tracedPoint(const SeriesData& data, const AxisMapping& x,
                                                      const AxisMapping& y, PixelBox box,
                                                      PixelPoint              position,
                                                      std::span<const double> sizes = {});

}  // namespace rocketplot::core
