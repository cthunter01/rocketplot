#pragma once

#include <QObject>
#include <QString>
#include <QTimeZone>
#include <Qt>
#include <optional>

#include "rocketplot/Range.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// One axis of a plot: its visible range, scale, labels and grid. Owned by its PlotWidget
/// (PlotWidget::xAxis(), yAxis() and the secondary yAxis2() on the right).
///
/// While autoscale is on (the default) the axis follows the data of the series drawn against it,
/// as autoscaleMode() says: all of it plus a margin, only what's inside the current x range, or the
/// newest followWindow() of it. Setting a range, or panning and zooming with the mouse, turns
/// autoscale off until it is turned back on, which a double-click on the plot does for all axes.
///
/// A DATE_TIME axis takes seconds since 1970-01-01 00:00 UTC (see plottime.h for conversions) and
/// labels them as dates and times in timeZone(), UTC by default.
class ROCKETPLOT_EXPORT Axis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(double min READ min WRITE setMin NOTIFY changed)
    Q_PROPERTY(double max READ max WRITE setMax NOTIFY changed)
    Q_PROPERTY(rocketplot::ScaleType scaleType READ scaleType WRITE setScaleType NOTIFY changed)
    Q_PROPERTY(rocketplot::NumberFormat numberFormat READ numberFormat WRITE setNumberFormat NOTIFY
                   changed)
    Q_PROPERTY(QTimeZone timeZone READ timeZone WRITE setTimeZone NOTIFY changed)
    Q_PROPERTY(bool autoscale READ autoscale WRITE setAutoscale NOTIFY changed)
    Q_PROPERTY(rocketplot::AutoscaleMode autoscaleMode READ autoscaleMode WRITE setAutoscaleMode
                   NOTIFY changed)
    Q_PROPERTY(double autoscaleMargin READ autoscaleMargin WRITE setAutoscaleMargin NOTIFY changed)
    Q_PROPERTY(double followWindow READ followWindow WRITE setFollowWindow NOTIFY changed)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY changed)
    Q_PROPERTY(
        bool minorGridVisible READ isMinorGridVisible WRITE setMinorGridVisible NOTIFY changed)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible RESET resetVisible NOTIFY changed)

public:
    ~Axis() override;
    Q_DISABLE_COPY_MOVE(Axis)

    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }
    /// Whether this is the plot's secondary y axis (on the right).
    [[nodiscard]] bool isSecondary() const noexcept { return m_secondary; }

    /// Text under the x axis or beside the y axis, e.g. "Time (s)". Qt rich text works too:
    /// "v<sub>z</sub> (m/s²)".
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    // Range
    // ----------------------------------------------------------------------------------------

    /// The visible range of data values.
    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] double min() const noexcept { return m_range.min; }
    [[nodiscard]] double max() const noexcept { return m_range.max; }
    /// Shows [min, max] and turns autoscale off. The ends are swapped if reversed; a range the axis
    /// can't show (not finite, too small to resolve, or not positive on a log scale) is ignored.
    void setRange(Range range);
    void setRange(double min, double max) { setRange(Range{.min = min, .max = max}); }
    void setMin(double min) { setRange(min, m_range.max); }
    void setMax(double max) { setRange(m_range.min, max); }

    // Scale and labels
    // -----------------------------------------------------------------------------

    [[nodiscard]] ScaleType scaleType() const noexcept { return m_scaleType; }
    /// Switching to LOGARITHMIC while the range includes values <= 0 turns autoscale back on.
    void setScaleType(ScaleType type);

    [[nodiscard]] NumberFormat numberFormat() const noexcept { return m_numberFormat; }
    void                       setNumberFormat(NumberFormat format);

    /// The time zone a DATE_TIME axis shows times in; UTC by default.
    [[nodiscard]] QTimeZone timeZone() const { return m_timeZone; }
    void                    setTimeZone(const QTimeZone& zone);

    // Autoscale
    // ------------------------------------------------------------------------------------

    /// Whether the range follows the data. Turning it on fits the data at once.
    [[nodiscard]] bool autoscale() const noexcept { return m_autoscale; }
    void               setAutoscale(bool enabled);

    [[nodiscard]] AutoscaleMode autoscaleMode() const noexcept { return m_autoscaleMode; }
    void                        setAutoscaleMode(AutoscaleMode mode);

    /// Space left around the data when autoscaling, as a fraction of the data's span on each side.
    [[nodiscard]] double autoscaleMargin() const noexcept { return m_autoscaleMargin; }
    void                 setAutoscaleMargin(double margin);

    /// How much data FOLLOW_LATEST shows, in data units (seconds on a DATE_TIME axis).
    [[nodiscard]] double followWindow() const noexcept { return m_followWindow; }
    void                 setFollowWindow(double window);

    // Appearance
    // -----------------------------------------------------------------------------------

    [[nodiscard]] bool isGridVisible() const noexcept { return m_gridVisible; }
    void               setGridVisible(bool visible);
    /// Lighter grid lines at the minor ticks. Off by default.
    [[nodiscard]] bool isMinorGridVisible() const noexcept { return m_minorGridVisible; }
    void               setMinorGridVisible(bool visible);

    /// Whether the axis is drawn. By default the x and y axes are, and the secondary y axis is
    /// drawn once a series uses it. setVisible() overrides that either way.
    [[nodiscard]] bool isVisible() const noexcept { return m_visible.value_or(!m_secondary); }
    void               setVisible(bool visible);
    void               resetVisible();
    /// Whether the axis is drawn, given whether any series uses it.
    [[nodiscard]] bool isShown(bool hasSeries) const noexcept;

Q_SIGNALS:
    /// The visible range changed, by the user, by autoscale or by setRange().
    void rangeChanged(double min, double max);
    /// Autoscale was switched on or off.
    void autoscaleChanged(bool enabled);
    /// Any property changed, the range included.
    void changed();
    /// Something that decides the autoscaled range changed (scale, mode, margin, window, or
    /// autoscale turned on); the plot refits.
    void fitNeeded();

private:
    friend class PlotWidget;
    friend class PlotLink;
    Axis(Qt::Orientation orientation, bool secondary, QObject* parent);
    // Sets the range without touching autoscale (autoscale itself uses this).
    void applyRange(Range range);
    // Sets autoscale without fitting (linked axes copy it).
    void applyAutoscale(bool enabled);
    // The settings a saved state holds (PlotWidget::saveState()), and taking them back.
    void saveState(QJsonObject& state) const;
    void restoreState(const QJsonObject& state);

    Qt::Orientation     m_orientation;
    bool                m_secondary;
    QString             m_label;
    Range               m_range{.min = 0.0, .max = 1.0};
    ScaleType           m_scaleType       = ScaleType::LINEAR;
    NumberFormat        m_numberFormat    = NumberFormat::AUTO;
    QTimeZone           m_timeZone        = QTimeZone::utc();
    bool                m_autoscale       = true;
    AutoscaleMode       m_autoscaleMode   = AutoscaleMode::FIT_ALL;
    double              m_autoscaleMargin = 0.03;
    double              m_followWindow    = 10.0;
    bool                m_gridVisible;
    bool                m_minorGridVisible = false;
    std::optional<bool> m_visible;
};

}  // namespace rocketplot
