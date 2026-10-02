#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/Range.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

namespace rocketplot
{

namespace core
{
class SeriesData;
}

class PlotWidget;

/// A data set drawn in a PlotWidget: the base of LineSeries and ScatterSeries. Series are created
/// by the plot (PlotWidget::addLine(), PlotWidget::addScatter()), which owns them.
///
/// Data is copied in by default, from any sized range of numbers, converted to double; a
/// std::vector<double> passed as an rvalue is moved in instead. setDataView() plots the caller's
/// own memory without copying: it must outlive its use by the series, and notifyDataChanged() must
/// follow any change made to it in place.
///
/// x values either come as an array or are implicit (UniformX: start + i * step, or the index when
/// y is all there is). Points with a NaN or infinite x or y are gaps: lines break there, and
/// autoscale ignores them.
///
/// Data functions throw std::invalid_argument when x and y differ in size. append() throws
/// std::logic_error on a view, or with the wrong kind of x (x values for a UniformX series, or none
/// for one with an x array).
class ROCKETPLOT_EXPORT Series : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor RESET resetColor NOTIFY changed)
    Q_PROPERTY(rocketplot::Marker marker READ marker WRITE setMarker NOTIFY changed)
    Q_PROPERTY(
        double markerSize READ markerSize WRITE setMarkerSize RESET resetMarkerSize NOTIFY changed)

public:
    ~Series() override;
    Q_DISABLE_COPY_MOVE(Series)

    /// The plot this series belongs to.
    [[nodiscard]] PlotWidget* plot() const noexcept { return m_plot; }

    /// The name in the legend. A series with an empty name has no legend entry.
    [[nodiscard]] QString name() const { return m_name; }
    void                  setName(const QString& name);

    [[nodiscard]] bool isVisible() const noexcept { return m_visible; }
    void               setVisible(bool visible);

    /// The series color: the one set, or else the plot theme's color for this series (which follows
    /// theme changes, e.g. to dark mode).
    [[nodiscard]] QColor color() const;
    void                 setColor(const QColor& color);
    /// Back to the theme's color.
    void resetColor();

    [[nodiscard]] Marker marker() const noexcept { return m_marker; }
    void                 setMarker(Marker marker);
    /// Marker diameter in device-independent pixels; by default the theme's.
    [[nodiscard]] double markerSize() const;
    void                 setMarkerSize(double size);
    void                 resetMarkerSize();

    // Data
    // -----------------------------------------------------------------------------------------------------

    /// Number of points.
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool        isEmpty() const noexcept { return size() == 0; }
    /// The x and y of point @p index (< size()).
    [[nodiscard]] double x(std::size_t index) const noexcept;
    [[nodiscard]] double y(std::size_t index) const noexcept;
    /// Bounds of the points whose x and y are both finite (Range::empty() when there are none).
    [[nodiscard]] Range xBounds() const noexcept;
    [[nodiscard]] Range yBounds() const noexcept;
    /// Whether x never decreases (and has no NaN). Sorted series pan and zoom fastest with millions
    /// of points.
    [[nodiscard]] bool isSortedByX() const noexcept;
    /// Whether x is implicit (UniformX) rather than an array.
    [[nodiscard]] bool hasUniformX() const noexcept;
    /// Whether the series reads the caller's memory (setDataView()) rather than its own copy.
    [[nodiscard]] bool isView() const noexcept;

    /// Copies @p x and @p y.
    template <NumericRange X, NumericRange Y>
    void setData(const X& x, const Y& y)
    {
        setData(detail::toDoubleVector(x), detail::toDoubleVector(y));
    }
    /// Takes over @p x and @p y without copying.
    void setData(std::vector<double>&& x, std::vector<double>&& y);
    /// Copies @p y; x is the index (0, 1, 2, ...).
    template <NumericRange Y>
    void setData(const Y& y)
    {
        setData(UniformX{}, detail::toDoubleVector(y));
    }
    /// Copies @p y, with x values start + i * step.
    template <NumericRange Y>
    void setData(UniformX x, const Y& y)
    {
        setData(x, detail::toDoubleVector(y));
    }
    void setData(UniformX x, std::vector<double>&& y);

    /// Plots @p x and @p y where they are, without copying.
    void setDataView(std::span<const double> x, std::span<const double> y);
    /// Plots @p y where it is, without copying, with x values start + i * step.
    void setDataView(UniformX x, std::span<const double> y);
    /// Call after changing the memory of a view in place.
    void notifyDataChanged();

    /// Adds points (to a series with an x array).
    template <NumericRange X, NumericRange Y>
    void append(const X& x, const Y& y)
    {
        if constexpr (detail::ContiguousDoubles<X> && detail::ContiguousDoubles<Y>)
        {
            appendPoints(std::span<const double>(x), std::span<const double>(y));
        }
        else
        {
            const std::vector<double> xs = detail::toDoubleVector(x);
            const std::vector<double> ys = detail::toDoubleVector(y);
            appendPoints(xs, ys);
        }
    }
    /// Adds one point (to a series with an x array).
    void append(double x, double y);
    /// Adds samples (to a UniformX series): their x values continue the sequence.
    template <NumericRange Y>
    void append(const Y& y)
    {
        if constexpr (detail::ContiguousDoubles<Y>)
        {
            appendSamples(std::span<const double>(y));
        }
        else
        {
            const std::vector<double> ys = detail::toDoubleVector(y);
            appendSamples(ys);
        }
    }
    /// Adds one sample (to a UniformX series).
    void append(double y);

    /// Removes every point, leaving an empty series with an x array.
    void clear();

Q_SIGNALS:
    /// A style property changed (name, visibility, color, marker, ...).
    void changed();
    /// The data was replaced, appended to or notified as changed.
    void dataChanged();

protected:
    explicit Series(PlotWidget* plot, Marker marker);

private:
    friend class PlotRenderer;
    friend class PlotWidget;

    void appendPoints(std::span<const double> x, std::span<const double> y);
    void appendSamples(std::span<const double> y);
    void dataWasChanged();
    [[nodiscard]] const core::SeriesData& data() const noexcept { return *m_data; }

    PlotWidget*                       m_plot;
    std::unique_ptr<core::SeriesData> m_data;
    QString                           m_name;
    bool                              m_visible = true;
    std::optional<QColor>             m_color;
    qsizetype                         m_colorIndex = 0;  // into the theme's series colors
    Marker                            m_marker;
    std::optional<double>             m_markerSize;
};

}  // namespace rocketplot
