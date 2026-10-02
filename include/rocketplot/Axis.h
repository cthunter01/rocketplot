#pragma once

#include <QObject>
#include <QString>
#include <Qt>

#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// One axis of a plot: its visible range, label and grid. Owned by its PlotWidget
/// (PlotWidget::xAxis(), PlotWidget::yAxis()).
///
/// While autoscale is on (the default) the axis follows the data: it shows every visible series,
/// plus a margin. Setting a range, or panning and zooming with the mouse, turns autoscale off until
/// it is turned back on, which a double-click on the plot does for both axes.
class ROCKETPLOT_EXPORT Axis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(double min READ min WRITE setMin NOTIFY changed)
    Q_PROPERTY(double max READ max WRITE setMax NOTIFY changed)
    Q_PROPERTY(bool autoscale READ autoscale WRITE setAutoscale NOTIFY changed)
    Q_PROPERTY(double autoscaleMargin READ autoscaleMargin WRITE setAutoscaleMargin NOTIFY changed)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY changed)

public:
    ~Axis() override;
    Q_DISABLE_COPY_MOVE(Axis)

    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }

    /// Text under the x axis or beside the y axis, e.g. "Time (s)".
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    /// The visible range of data values.
    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] double min() const noexcept { return m_range.min; }
    [[nodiscard]] double max() const noexcept { return m_range.max; }
    /// Shows [min, max] and turns autoscale off. The ends are swapped if reversed; a range that
    /// can't be shown (not finite, or too small to resolve) is ignored.
    void setRange(Range range);
    void setRange(double min, double max) { setRange(Range{.min = min, .max = max}); }
    void setMin(double min) { setRange(min, m_range.max); }
    void setMax(double max) { setRange(m_range.min, max); }

    /// Whether the range follows the data. Turning it on fits the data at once.
    [[nodiscard]] bool autoscale() const noexcept { return m_autoscale; }
    void               setAutoscale(bool enabled);

    /// Space left around the data when autoscaling, as a fraction of the data's span on each side.
    [[nodiscard]] double autoscaleMargin() const noexcept { return m_autoscaleMargin; }
    void                 setAutoscaleMargin(double margin);

    [[nodiscard]] bool isGridVisible() const noexcept { return m_gridVisible; }
    void               setGridVisible(bool visible);

Q_SIGNALS:
    /// The visible range changed, by the user, by autoscale or by setRange().
    void rangeChanged(double min, double max);
    /// Any property changed, the range included.
    void changed();
    /// Autoscale was switched on; the plot refits the data.
    void autoscaleEnabled();

private:
    friend class PlotWidget;
    Axis(Qt::Orientation orientation, QObject* parent);
    // Sets the range without touching autoscale (autoscale itself uses this).
    void applyRange(Range range);

    Qt::Orientation m_orientation;
    QString         m_label;
    Range           m_range{.min = 0.0, .max = 1.0};
    bool            m_autoscale       = true;
    double          m_autoscaleMargin = 0.03;
    bool            m_gridVisible     = true;
};

}  // namespace rocketplot
