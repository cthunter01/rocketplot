#pragma once

#include <QColor>
#include <QObject>
#include <Qt>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "rocketplot/BoxStatistics.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// A box and whiskers: what BoxStatistics says of a set of values, at a position along the other
/// axis. Created by PlotWidget::addBox(). On a CATEGORY axis (Axis::setCategories()) the position
/// is the index of the category the box stands for.
///
/// @code
/// plot->xAxis()->setScaleType(rocketplot::ScaleType::CATEGORY);
/// plot->xAxis()->setCategories({"Nominal", "Gusty"});
/// plot->addBox(rocketplot::BoxStatistics::fromValues(nominal), 0, "Nominal");
/// plot->addBox(rocketplot::BoxStatistics::fromValues(gusty), 1, "Gusty");
/// @endcode
///
/// Its points are the statistics, in the order of BoxPart (the lower whisker, the quartiles and
/// the median, the upper whisker, the mean if there is one, the outliers, the far-outs), at the
/// position: that is where a crosshair snaps to (partAt() says what it is on), what the legend
/// reads (the median, over the box) and what the CSV export writes, with a column naming the
/// parts. They change through setStatistics() and setPosition() only: the data and error functions
/// throw std::logic_error. Outliers are drawn with marker() (a hollow circle by default),
/// far-outs with farOutMarker(). Autoscale makes room for all of them, and for the box's width.
class ROCKETPLOT_EXPORT BoxSeries : public Series
{
    Q_OBJECT
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY changed)
    Q_PROPERTY(double width READ width WRITE setWidth RESET resetWidth NOTIFY changed)
    Q_PROPERTY(double fillOpacity READ fillOpacity WRITE setFillOpacity RESET resetFillOpacity
                   NOTIFY changed)
    Q_PROPERTY(
        double lineWidth READ lineWidth WRITE setLineWidth RESET resetLineWidth NOTIFY changed)
    Q_PROPERTY(bool medianVisible READ isMedianVisible WRITE setMedianVisible NOTIFY changed)
    Q_PROPERTY(QColor medianColor READ medianColor WRITE setMedianColor RESET resetMedianColor
                   NOTIFY changed)
    Q_PROPERTY(bool meanVisible READ isMeanVisible WRITE setMeanVisible NOTIFY changed)
    Q_PROPERTY(double whiskerCapRatio READ whiskerCapRatio WRITE setWhiskerCapRatio RESET
                   resetWhiskerCapRatio NOTIFY changed)
    Q_PROPERTY(
        rocketplot::Marker farOutMarker READ farOutMarker WRITE setFarOutMarker NOTIFY changed)

public:
    ~BoxSeries() override;
    Q_DISABLE_COPY_MOVE(BoxSeries)

    /// Throws std::invalid_argument unless the five numbers are finite and in order (lower
    /// whisker <= first quartile <= median <= third quartile <= upper whisker); a mean, outliers
    /// and far-outs that aren't finite are left out.
    void                               setStatistics(const BoxStatistics& statistics);
    [[nodiscard]] const BoxStatistics& statistics() const noexcept { return m_statistics; }

    /// Where the box stands along the other axis: x for an upright box, y for one lying down.
    [[nodiscard]] double position() const noexcept { return m_position; }
    void                 setPosition(double position);

    /// What point @p index (< size()) stands for.
    [[nodiscard]] BoxPart partAt(std::size_t index) const;

    /// Qt::Vertical (the default): the box stands upright at an x, its values along y.
    /// Qt::Horizontal: it lies at a y, its values along x.
    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }
    void                          setOrientation(Qt::Orientation orientation);

    /// The box's width across, in the units of the position's axis (0.5 by default: half the
    /// distance between categories).
    [[nodiscard]] double width() const noexcept { return m_width; }
    void                 setWidth(double width);
    void                 resetWidth() { setWidth(0.5); }

    /// How opaque the box's fill is, from 0 to 1 (0.25 by default).
    [[nodiscard]] double fillOpacity() const noexcept { return m_fillOpacity; }
    void                 setFillOpacity(double opacity);
    void                 resetFillOpacity() { setFillOpacity(0.25); }

    /// The width of the box's outline, its whiskers and the median, in device-independent pixels;
    /// by default the theme's line width.
    [[nodiscard]] double lineWidth() const;
    void                 setLineWidth(double width);
    void                 resetLineWidth();

    /// The median across the box (shown by default), in medianColor(): the series color unless
    /// set.
    [[nodiscard]] bool   isMedianVisible() const noexcept { return m_medianVisible; }
    void                 setMedianVisible(bool visible);
    [[nodiscard]] QColor medianColor() const;
    void                 setMedianColor(const QColor& color);
    void                 resetMedianColor();

    /// The mean as a diamond (hidden by default; shown only where the statistics have one).
    [[nodiscard]] bool isMeanVisible() const noexcept { return m_meanVisible; }
    void               setMeanVisible(bool visible);

    /// The width of the caps at the whiskers' ends as a share of the box's width (0.5 by default;
    /// 0 for none).
    [[nodiscard]] double whiskerCapRatio() const noexcept { return m_whiskerCapRatio; }
    void                 setWhiskerCapRatio(double ratio);
    void                 resetWhiskerCapRatio() { setWhiskerCapRatio(0.5); }

    /// The marker of the far-outs (Marker::CROSS by default); the outliers have marker().
    [[nodiscard]] Marker farOutMarker() const noexcept { return m_farOutMarker; }
    void                 setFarOutMarker(Marker marker);

protected:
    void saveState(QJsonObject& state) const override;
    void restoreState(const QJsonObject& state) override;

private:
    friend class PlotRenderer;
    friend class PlotWidget;
    explicit BoxSeries(PlotWidget* plot);

    // The points again, from the statistics, the position and the orientation.
    void                rebuild();
    [[nodiscard]] Range across() const;  // the box's extent along the position axis
    [[nodiscard]] Range fitBoundsX(bool positiveOnly) const override;
    [[nodiscard]] Range fitBoundsY(bool positiveOnly) const override;
    [[nodiscard]] Range fitBoundsYWithin(Range xRange, bool positiveOnly) const override;
    [[nodiscard]] std::optional<std::size_t> indexNear(double x) const override;
    [[nodiscard]] std::optional<double>      yAt(double x) const override;
    void                                     addCsvColumns(core::CsvSeries& columns) const override;

    BoxStatistics            m_statistics;
    std::vector<BoxPart>     m_parts;      // of each point
    std::vector<std::string> m_partNames;  // of each point, for the CSV
    double                   m_position    = 0.0;
    Qt::Orientation          m_orientation = Qt::Vertical;
    double                   m_width       = 0.5;
    double                   m_fillOpacity = 0.25;
    std::optional<double>    m_lineWidth;
    bool                     m_medianVisible = true;
    std::optional<QColor>    m_medianColor;
    bool                     m_meanVisible     = false;
    double                   m_whiskerCapRatio = 0.5;
    Marker                   m_farOutMarker    = Marker::CROSS;
};

}  // namespace rocketplot
