#include "rocketplot/BoxSeries.h"

#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <Qt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotState.h"
#include "core/CsvWriter.h"
#include "core/SeriesData.h"
#include "rocketplot/BoxStatistics.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

const char* partName(BoxPart part)
{
    switch (part)
    {
        case BoxPart::LOWER_WHISKER:
            return "lower whisker";
        case BoxPart::FIRST_QUARTILE:
            return "first quartile";
        case BoxPart::MEDIAN:
            return "median";
        case BoxPart::THIRD_QUARTILE:
            return "third quartile";
        case BoxPart::UPPER_WHISKER:
            return "upper whisker";
        case BoxPart::MEAN:
            return "mean";
        case BoxPart::OUTLIER:
            return "outlier";
        case BoxPart::FAR_OUT:
            return "far-out";
    }
    return "";
}

std::vector<double> finiteOnly(const std::vector<double>& values)
{
    std::vector<double> finite;
    std::ranges::copy_if(values, std::back_inserter(finite),
                         [](double value) { return std::isfinite(value); });
    return finite;
}

}  // namespace

BoxSeries::BoxSeries(PlotWidget* plot)
  : Series(plot, SeriesKind::BOX, Marker::CIRCLE, ErrorStyle::BARS)
{
    lockData(true);
    setMarkerFilled(false);  // outliers as hollow circles
}

BoxSeries::~BoxSeries() = default;

void BoxSeries::setStatistics(const BoxStatistics& statistics)
{
    const std::array five{statistics.lowerWhisker, statistics.q1, statistics.median, statistics.q3,
                          statistics.upperWhisker};
    if (!std::ranges::all_of(five, [](double value) { return std::isfinite(value); }) ||
        !std::ranges::is_sorted(five))
    {
        throw std::invalid_argument(
            "rocketplot::BoxSeries::setStatistics: the whiskers, quartiles "
            "and median must be finite and in order");
    }
    m_statistics          = statistics;
    m_statistics.outliers = finiteOnly(statistics.outliers);
    m_statistics.farOuts  = finiteOnly(statistics.farOuts);
    if (m_statistics.mean && !std::isfinite(*m_statistics.mean))
    {
        m_statistics.mean.reset();
    }
    rebuild();
}

void BoxSeries::setPosition(double position)
{
    if (!std::isfinite(position))
    {
        qCWarning(lcData) << "BoxSeries::setPosition: ignoring" << position;
        return;
    }
    if (position == m_position)
    {
        return;
    }
    m_position = position;
    rebuild();
    Q_EMIT changed();
}

BoxPart BoxSeries::partAt(std::size_t index) const
{
    return index < m_parts.size() ? m_parts[index] : BoxPart::MEDIAN;
}

void BoxSeries::setOrientation(Qt::Orientation orientation)
{
    if (orientation == m_orientation)
    {
        return;
    }
    m_orientation = orientation;
    rebuild();
    Q_EMIT changed();
}

void BoxSeries::setWidth(double width)
{
    if (!std::isfinite(width) || !(width > 0.0))
    {
        qCWarning(lcData) << "BoxSeries::setWidth: ignoring" << width;
        return;
    }
    if (width == m_width)
    {
        return;
    }
    m_width = width;
    Q_EMIT changed();
    Q_EMIT dataChanged();  // autoscale makes room for it
}

void BoxSeries::setFillOpacity(double opacity)
{
    if (!std::isfinite(opacity))
    {
        qCWarning(lcData) << "BoxSeries::setFillOpacity: ignoring" << opacity;
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (opacity == m_fillOpacity)
    {
        return;
    }
    m_fillOpacity = opacity;
    Q_EMIT changed();
}

double BoxSeries::lineWidth() const
{
    return m_lineWidth.value_or(plot()->theme().lineWidth);
}

void BoxSeries::setLineWidth(double width)
{
    if (!std::isfinite(width) || !(width > 0.0))
    {
        qCWarning(lcData) << "BoxSeries::setLineWidth: ignoring" << width;
        return;
    }
    if (m_lineWidth == width)
    {
        return;
    }
    m_lineWidth = width;
    Q_EMIT changed();
}

void BoxSeries::resetLineWidth()
{
    if (!m_lineWidth)
    {
        return;
    }
    m_lineWidth.reset();
    Q_EMIT changed();
}

void BoxSeries::setMedianVisible(bool visible)
{
    if (visible == m_medianVisible)
    {
        return;
    }
    m_medianVisible = visible;
    Q_EMIT changed();
}

QColor BoxSeries::medianColor() const
{
    return m_medianColor.value_or(color());
}

void BoxSeries::setMedianColor(const QColor& color)
{
    if (m_medianColor == color)
    {
        return;
    }
    m_medianColor = color;
    Q_EMIT changed();
}

void BoxSeries::resetMedianColor()
{
    if (!m_medianColor)
    {
        return;
    }
    m_medianColor.reset();
    Q_EMIT changed();
}

void BoxSeries::setMeanVisible(bool visible)
{
    if (visible == m_meanVisible)
    {
        return;
    }
    m_meanVisible = visible;
    Q_EMIT changed();
}

void BoxSeries::setWhiskerCapRatio(double ratio)
{
    if (!std::isfinite(ratio))
    {
        qCWarning(lcData) << "BoxSeries::setWhiskerCapRatio: ignoring" << ratio;
        return;
    }
    ratio = std::clamp(ratio, 0.0, 1.0);
    if (ratio == m_whiskerCapRatio)
    {
        return;
    }
    m_whiskerCapRatio = ratio;
    Q_EMIT changed();
}

void BoxSeries::setFarOutMarker(Marker marker)
{
    if (marker == m_farOutMarker)
    {
        return;
    }
    m_farOutMarker = marker;
    Q_EMIT changed();
}

void BoxSeries::rebuild()
{
    const BoxStatistics& box = m_statistics;
    std::vector<double>  values{box.lowerWhisker, box.q1, box.median, box.q3, box.upperWhisker};
    m_parts = {BoxPart::LOWER_WHISKER, BoxPart::FIRST_QUARTILE, BoxPart::MEDIAN,
               BoxPart::THIRD_QUARTILE, BoxPart::UPPER_WHISKER};
    if (box.mean)
    {
        values.push_back(*box.mean);
        m_parts.push_back(BoxPart::MEAN);
    }
    for (const double value : box.outliers)
    {
        values.push_back(value);
        m_parts.push_back(BoxPart::OUTLIER);
    }
    for (const double value : box.farOuts)
    {
        values.push_back(value);
        m_parts.push_back(BoxPart::FAR_OUT);
    }
    m_partNames.clear();
    for (const BoxPart part : m_parts)
    {
        m_partNames.emplace_back(partName(part));
    }
    std::vector<double> positions(values.size(), m_position);
    if (m_orientation == Qt::Vertical)
    {
        replacePoints(std::move(positions), std::move(values));
    }
    else
    {
        replacePoints(std::move(values), std::move(positions));
    }
}

Range BoxSeries::across() const
{
    return {.min = m_position - (m_width / 2.0), .max = m_position + (m_width / 2.0)};
}

namespace
{

// The part of @p range a log axis can show.
Range positivePart(Range range, bool positiveOnly)
{
    if (!positiveOnly)
    {
        return range;
    }
    if (!(range.max > 0.0))
    {
        return Range::empty();
    }
    return {.min = std::max(range.min, std::nextafter(0.0, 1.0)), .max = range.max};
}

}  // namespace

Range BoxSeries::fitBoundsX(bool positiveOnly) const
{
    if (isEmpty())
    {
        return Range::empty();
    }
    if (m_orientation == Qt::Vertical)
    {
        return positivePart(across(), positiveOnly);
    }
    return positiveOnly ? data().xPositiveBounds() : data().xBounds();
}

Range BoxSeries::fitBoundsY(bool positiveOnly) const
{
    if (isEmpty())
    {
        return Range::empty();
    }
    if (m_orientation == Qt::Horizontal)
    {
        return positivePart(across(), positiveOnly);
    }
    return positiveOnly ? data().yPositiveBounds() : data().yBounds();
}

Range BoxSeries::fitBoundsYWithin(Range xRange, bool positiveOnly) const
{
    if (isEmpty())
    {
        return Range::empty();
    }
    if (m_orientation == Qt::Vertical)
    {
        const Range box = across();
        return box.max < xRange.min || box.min > xRange.max ? Range::empty()
                                                            : fitBoundsY(positiveOnly);
    }
    // Lying down: as tall as it is wherever any of it is in view.
    const Range values = data().xBounds();
    return values.max < xRange.min || values.min > xRange.max
               ? Range::empty()
               : positivePart(across(), positiveOnly);
}

std::optional<std::size_t> BoxSeries::indexNear(double x) const
{
    // Over an upright box, its median.
    if (isEmpty() || m_orientation != Qt::Vertical || !across().contains(x))
    {
        return std::nullopt;
    }
    return std::size_t{2};
}

std::optional<double> BoxSeries::yAt(double x) const
{
    const std::optional<std::size_t> median = indexNear(x);
    return median ? std::optional(y(*median)) : std::nullopt;
}

void BoxSeries::addCsvColumns(core::CsvSeries& columns) const
{
    columns.labels = m_partNames;
}

void BoxSeries::saveState(QJsonObject& state) const
{
    Series::saveState(state);
    state.insert(QLatin1String("orientation"), state::fromEnum(m_orientation));
    state.insert(QLatin1String("width"), m_width);
    state.insert(QLatin1String("fillOpacity"), m_fillOpacity);
    state.insert(QLatin1String("lineWidth"), state::fromOptional(m_lineWidth));
    state.insert(QLatin1String("medianVisible"), m_medianVisible);
    state.insert(QLatin1String("medianColor"),
                 m_medianColor ? state::fromColor(*m_medianColor) : QJsonValue(QJsonValue::Null));
    state.insert(QLatin1String("meanVisible"), m_meanVisible);
    state.insert(QLatin1String("whiskerCapRatio"), m_whiskerCapRatio);
    state.insert(QLatin1String("farOutMarker"), state::fromEnum(m_farOutMarker));
}

void BoxSeries::restoreState(const QJsonObject& state)
{
    Series::restoreState(state);
    if (const auto orientation =
            state::toEnum<Qt::Orientation>(state.value(QLatin1String("orientation"))))
    {
        setOrientation(*orientation);
    }
    if (const auto width = state::toNumber(state.value(QLatin1String("width"))))
    {
        setWidth(*width);
    }
    if (const auto opacity = state::toNumber(state.value(QLatin1String("fillOpacity"))))
    {
        setFillOpacity(*opacity);
    }
    state::restoreOptional(
        state.value(QLatin1String("lineWidth")), state::toNumber,
        [this](double width) { setLineWidth(width); }, [this] { resetLineWidth(); });
    if (const auto visible = state::toBool(state.value(QLatin1String("medianVisible"))))
    {
        setMedianVisible(*visible);
    }
    state::restoreOptional(
        state.value(QLatin1String("medianColor")), state::toColor,
        [this](const QColor& color) { setMedianColor(color); }, [this] { resetMedianColor(); });
    if (const auto visible = state::toBool(state.value(QLatin1String("meanVisible"))))
    {
        setMeanVisible(*visible);
    }
    if (const auto ratio = state::toNumber(state.value(QLatin1String("whiskerCapRatio"))))
    {
        setWhiskerCapRatio(*ratio);
    }
    if (const auto marker = state::toEnum<Marker>(state.value(QLatin1String("farOutMarker"))))
    {
        setFarOutMarker(*marker);
    }
}

}  // namespace rocketplot
