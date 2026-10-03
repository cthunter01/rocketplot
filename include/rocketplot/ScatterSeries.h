#pragma once

#include <QColor>
#include <QObject>
#include <QRgb>
#include <Qt>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/Series.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// Colors a scatter series copies from: any sized range of QColor, QRgb or Qt::GlobalColor.
template <class R>
concept ColorRange =
    std::ranges::forward_range<R> && std::ranges::sized_range<R> &&
    (std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, QColor> ||
     std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, QRgb> ||
     std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, Qt::GlobalColor>);

/// A series drawn as unconnected markers (circles by default). Created by PlotWidget::addScatter().
///
/// Every marker has the series' markerSize() and color() unless the points are given their own
/// (setSizes(), setColors()): a third and fourth value per point, shown as a bubble's size or a
/// color scale. Like errors, these belong to the points the series has when they are set:
/// replacing the data removes them, and points appended later use the series' size and color.
/// They throw std::invalid_argument unless there is one value per point.
class ROCKETPLOT_EXPORT ScatterSeries : public Series
{
    Q_OBJECT

public:
    ~ScatterSeries() override;
    Q_DISABLE_COPY_MOVE(ScatterSeries)

    /// Gives each point its own marker diameter, in device-independent pixels, copied. A point
    /// whose size isn't a positive number isn't drawn. For a value shown as the marker's area,
    /// pass its square root.
    template <NumericRange S>
    void setSizes(const S& sizes)
    {
        setSizes(detail::toDoubleVector(sizes));
    }
    /// Takes over @p sizes without copying.
    void setSizes(std::vector<double>&& sizes);
    /// Back to markerSize() for every point.
    void               clearSizes();
    [[nodiscard]] bool hasSizes() const noexcept { return !m_sizes.empty(); }
    /// The marker diameter of point @p index.
    [[nodiscard]] double pointSize(std::size_t index) const;

    /// Gives each point its own color, copied. A QRgb is taken as opaque, as QColor does; for
    /// translucent markers pass QColor.
    template <ColorRange C>
    void setColors(const C& colors)
    {
        std::vector<QRgb> values;
        values.reserve(static_cast<std::size_t>(std::ranges::size(colors)));
        for (const auto& color : colors)
        {
            values.push_back(QColor(color).rgba());
        }
        applyColors(std::move(values));
    }
    /// Back to color() for every point.
    void               clearColors();
    [[nodiscard]] bool hasColors() const noexcept { return !m_colors.empty(); }
    /// The marker color of point @p index.
    [[nodiscard]] QColor pointColor(std::size_t index) const;

private:
    friend class PlotRenderer;
    friend class PlotWidget;
    explicit ScatterSeries(PlotWidget* plot);

    void                                  applyColors(std::vector<QRgb>&& colors);
    void                                  pointsReplaced() override;
    [[nodiscard]] std::span<const double> sizes() const noexcept { return m_sizes; }
    [[nodiscard]] std::span<const QRgb>   colors() const noexcept { return m_colors; }

    std::vector<double> m_sizes;  // one per point they were set for; 0 for a hidden point
    std::vector<QRgb>   m_colors;
    double              m_largestSize = 0.0;
};

}  // namespace rocketplot
