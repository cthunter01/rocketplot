#pragma once

#include <QObject>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// Bars standing on 0, one per bin between two edges: n bars have n + 1 edges, in increasing
/// order, and a height each. Created by PlotWidget::addBars(); a histogram (HistogramSeries) is
/// one whose bars are counted from values.
///
/// @code
/// // Thrust bins of 50 N, and how many firings fell in each.
/// rocketplot::BarSeries* firings = plot->addBars(std::vector{800, 850, 900, 950},
///                                                std::vector{3, 11, 6}, "Firings");
/// firings->setOutlineVisible(true);
/// @endcode
///
/// Its points are the bars: x() is the middle of a bin and y() its height, which is what the
/// crosshair goes to, the legend reads and the CSV export writes (with each bin's edges). They
/// change through setBars() only: setData(), setDataView(), append() and clear() throw
/// std::logic_error. A bar can have errors along y like any point (setYErrors()). Autoscale shows
/// the bars from 0, with no margin below it.
class ROCKETPLOT_EXPORT BarSeries : public Series
{
    Q_OBJECT
    Q_PROPERTY(double fillOpacity READ fillOpacity WRITE setFillOpacity RESET resetFillOpacity
                   NOTIFY changed)
    Q_PROPERTY(bool outlineVisible READ isOutlineVisible WRITE setOutlineVisible NOTIFY changed)
    Q_PROPERTY(double barGap READ barGap WRITE setBarGap RESET resetBarGap NOTIFY changed)

public:
    ~BarSeries() override;
    Q_DISABLE_COPY_MOVE(BarSeries)

    /// Copies @p edges (n + 1 of them) and @p heights (n). Throws std::invalid_argument unless the
    /// counts match and the edges are finite and strictly increasing (both may be empty); a NaN
    /// height leaves its bar out. A histogram's bars come from its values: on one this throws
    /// std::logic_error.
    template <NumericRange E, NumericRange H>
    void setBars(const E& edges, const H& heights)
    {
        setBars(detail::toDoubleVector(edges), detail::toDoubleVector(heights));
    }
    void setBars(std::vector<double>&& edges, std::vector<double>&& heights);

    /// The edges of the bins: one more than there are bars.
    [[nodiscard]] std::span<const double> edges() const noexcept { return m_edges; }
    /// The bin of bar @p index (< size()): from its left edge to its right one.
    [[nodiscard]] Range bin(std::size_t index) const;
    /// The bar whose bin holds @p x (an edge belongs to the bin above it, the last edge to the last
    /// bin); nothing outside the bins.
    [[nodiscard]] std::optional<std::size_t> binAt(double x) const;

    /// How opaque the bars' fill is, from 0 to 1 (1, the default: solid).
    [[nodiscard]] double fillOpacity() const noexcept { return m_fillOpacity; }
    void                 setFillOpacity(double opacity);
    void                 resetFillOpacity() { setFillOpacity(1.0); }
    /// Whether each bar has an outline in the series color (off by default), which tells touching
    /// bars of a light fill apart.
    [[nodiscard]] bool isOutlineVisible() const noexcept { return m_outlineVisible; }
    void               setOutlineVisible(bool visible);
    /// The share of each bin's width left empty between neighbours, from 0 (the default: bars
    /// touch, as a histogram's do) to 0.9.
    [[nodiscard]] double barGap() const noexcept { return m_barGap; }
    void                 setBarGap(double gap);
    void                 resetBarGap() { setBarGap(0.0); }

protected:
    explicit BarSeries(PlotWidget* plot);

    void saveState(QJsonObject& state) const override;
    void restoreState(const QJsonObject& state) override;
    // Bars the series worked out itself, past the check a histogram makes.
    void applyBars(std::vector<double>&& edges, std::vector<double>&& heights);
    // Whether setBars() is for the user (not for a histogram).
    [[nodiscard]] virtual bool takesBars() const noexcept { return true; }

private:
    friend class PlotRenderer;
    friend class PlotWidget;

    [[nodiscard]] Range            fitBoundsX(bool positiveOnly) const override;
    [[nodiscard]] Range            fitBoundsY(bool positiveOnly) const override;
    [[nodiscard]] Range            fitBoundsYWithin(Range xRange, bool positiveOnly) const override;
    [[nodiscard]] Qt::Orientations zeroBaselines() const override;
    [[nodiscard]] bool pick(const core::PickQuery& query, core::NearestPoint& found) const override;
    [[nodiscard]] std::optional<std::size_t> indexNear(double x) const override;
    [[nodiscard]] std::optional<double>      yAt(double x) const override;
    void                                     addCsvColumns(core::CsvSeries& columns) const override;

    std::vector<double> m_edges;
    double              m_fillOpacity    = 1.0;
    bool                m_outlineVisible = false;
    double              m_barGap         = 0.0;
};

}  // namespace rocketplot
