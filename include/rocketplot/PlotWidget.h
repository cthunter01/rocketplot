#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QContextMenuEvent;
class QEvent;
class QIcon;
class QMenu;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace rocketplot
{

class Axis;
class InputBindings;
class InteractionController;
class Legend;
class PlotLink;
class LineSeries;
class ScatterSeries;
class Series;
class ViewHistory;
struct LegendEntry;

/// A widget that plots any number of data sets on shared x and y axes, with a legend.
///
/// @code
/// auto* plot = new rocketplot::PlotWidget(parent);
/// plot->setTitle("Ascent");
/// plot->xAxis()->setLabel("Time (s)");
/// plot->yAxis()->setLabel("Altitude (km)");
/// plot->addLine(time, stage1, "Stage 1");    // any sized ranges of numbers: copied as double
/// plot->addLine(time, stage2, "Stage 2");
/// @endcode
///
/// Drag to pan, Shift-drag to zoom to a box, and use the wheel to zoom about the pointer; over an
/// axis, these only affect that axis, and with Ctrl (x) or Shift (y) held the wheel zooms one axis.
/// On a trackpad, scroll to pan and pinch to zoom; on a touchscreen, drag and pinch. A double-click
/// returns to autoscale, and back() and forward() (the mouse's back and forward buttons, and the
/// context menu) step through the views the user went through. setInputBindings() changes which
/// gesture does what.
///
/// Series are owned by the plot. Data rules (copy vs view, UniformX, gaps, exceptions) are
/// described on Series.
class ROCKETPLOT_EXPORT PlotWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(
        rocketplot::ThemeMode themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)
    Q_PROPERTY(bool debugOverlay READ debugOverlay WRITE setDebugOverlay NOTIFY debugOverlayChanged)
    Q_PROPERTY(bool crosshairEnabled READ isCrosshairEnabled WRITE setCrosshairEnabled NOTIFY
                   crosshairEnabledChanged)

public:
    explicit PlotWidget(QWidget* parent = nullptr);
    ~PlotWidget() override;
    Q_DISABLE_COPY_MOVE(PlotWidget)

    // Lines
    // ----------------------------------------------------------------------------------------------------

    /// A line through (x[i], y[i]), copied.
    template <NumericRange X, NumericRange Y>
    LineSeries* addLine(const X& x, const Y& y, const QString& name = {})
    {
        return addLine(detail::toDoubleVector(x), detail::toDoubleVector(y), name);
    }
    /// A line through (x[i], y[i]), moved in without copying.
    LineSeries* addLine(std::vector<double>&& x, std::vector<double>&& y, const QString& name = {});
    /// A line through (i, y[i]), copied.
    template <NumericRange Y>
    LineSeries* addLine(const Y& y, const QString& name = {})
    {
        return addLine(UniformX{}, detail::toDoubleVector(y), name);
    }
    /// A line through (x.start + i * x.step, y[i]), copied.
    template <NumericRange Y>
    LineSeries* addLine(UniformX x, const Y& y, const QString& name = {})
    {
        return addLine(x, detail::toDoubleVector(y), name);
    }
    LineSeries* addLine(UniformX x, std::vector<double>&& y, const QString& name = {});
    /// A line through the caller's memory, without copying (see Series::setDataView()).
    LineSeries* addLineView(std::span<const double> x, std::span<const double> y,
                            const QString& name = {});
    LineSeries* addLineView(UniformX x, std::span<const double> y, const QString& name = {});

    // Scatter
    // --------------------------------------------------------------------------------------------------

    /// Markers at (x[i], y[i]), copied.
    template <NumericRange X, NumericRange Y>
    ScatterSeries* addScatter(const X& x, const Y& y, const QString& name = {})
    {
        return addScatter(detail::toDoubleVector(x), detail::toDoubleVector(y), name);
    }
    ScatterSeries* addScatter(std::vector<double>&& x, std::vector<double>&& y,
                              const QString& name = {});
    /// Markers at (i, y[i]), copied.
    template <NumericRange Y>
    ScatterSeries* addScatter(const Y& y, const QString& name = {})
    {
        return addScatter(UniformX{}, detail::toDoubleVector(y), name);
    }
    template <NumericRange Y>
    ScatterSeries* addScatter(UniformX x, const Y& y, const QString& name = {})
    {
        return addScatter(x, detail::toDoubleVector(y), name);
    }
    ScatterSeries* addScatter(UniformX x, std::vector<double>&& y, const QString& name = {});
    ScatterSeries* addScatterView(std::span<const double> x, std::span<const double> y,
                                  const QString& name = {});
    ScatterSeries* addScatterView(UniformX x, std::span<const double> y, const QString& name = {});

    // Series
    // ---------------------------------------------------------------------------------------------------

    /// Every series, in the order added (later ones are drawn on top).
    [[nodiscard]] QList<Series*> series() const;
    /// Removes and deletes @p series (if it belongs to this plot).
    void removeSeries(Series* series);
    /// Removes and deletes every series.
    void clearSeries();

    // Axes, legend, title
    // --------------------------------------------------------------------------------------

    [[nodiscard]] Axis* xAxis() const noexcept;
    [[nodiscard]] Axis* yAxis() const noexcept;
    /// The secondary y axis, on the right. Shown once a series uses it (Series::setYAxis()).
    [[nodiscard]] Axis*   yAxis2() const noexcept;
    [[nodiscard]] Legend* legend() const noexcept;
    /// The link this plot's x axis is tied to, if any (see PlotLink).
    [[nodiscard]] PlotLink* link() const noexcept;

    [[nodiscard]] QString title() const;
    void                  setTitle(const QString& title);

    // View
    // -----------------------------------------------------------------------------------------------------

    /// Turns autoscale back on for every axis, fitting the data. Recorded in the view history.
    void resetView();

    /// The view before the last pan, zoom or reset by the user (or resetView()), and back again. A
    /// view is every axis's range and autoscale setting; linked plots (PlotLink) share one history,
    /// so going back undoes a pan in whichever of them it happened. Ranges set in code aren't
    /// recorded.
    void               back();
    void               forward();
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;

    // Interaction
    // ----------------------------------------------------------------------------------------------

    /// Which gestures pan, zoom and so on. InputBindings::defaults() to begin with.
    [[nodiscard]] const InputBindings& inputBindings() const noexcept;
    void                               setInputBindings(const InputBindings& bindings);

    /// Draws lines through the pointer over the plot area, with its coordinates in tags on the
    /// axes. Linked plots (PlotLink) show a line at the same x. Off by default; the context menu
    /// has a switch for it.
    [[nodiscard]] bool isCrosshairEnabled() const noexcept;
    void               setCrosshairEnabled(bool enabled);
    /// Where the crosshair is, in data coordinates (y on yAxis()), or nothing when it isn't shown.
    /// On a plot showing a linked plot's crosshair, y is NaN.
    [[nodiscard]] std::optional<QPointF> crosshairPosition() const;

    // Appearance
    // -----------------------------------------------------------------------------------------------

    [[nodiscard]] ThemeMode themeMode() const noexcept;
    /// SYSTEM, LIGHT or DARK pick a built-in theme; CUSTOM keeps the current one.
    void setThemeMode(ThemeMode mode);
    /// The theme in use.
    [[nodiscard]] const Theme& theme() const noexcept;
    /// Uses @p theme (and switches to ThemeMode::CUSTOM).
    void setTheme(const Theme& theme);

    /// Draws layout boxes, frame time and point counts over the plot. Also on when the environment
    /// variable ROCKETPLOT_DEBUG_OVERLAY is 1.
    [[nodiscard]] bool debugOverlay() const noexcept;
    void               setDebugOverlay(bool enabled);

    // Geometry
    // -------------------------------------------------------------------------------------------------

    /// The area inside the axes where data is drawn, in widget coordinates.
    [[nodiscard]] QRectF plotArea() const;
    /// Where the legend was last drawn, in widget coordinates; empty when it isn't shown.
    [[nodiscard]] QRectF legendArea() const;
    /// The data coordinates at a widget position, and back, using @p yAxis (default: yAxis()).
    [[nodiscard]] QPointF mapToData(QPointF widgetPosition, const Axis* yAxis = nullptr) const;
    [[nodiscard]] QPointF mapFromData(QPointF dataPosition, const Axis* yAxis = nullptr) const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void titleChanged();
    void themeChanged();
    void debugOverlayChanged();
    void crosshairEnabledChanged();
    /// The crosshair moved, appeared or disappeared, or the data under it moved (see
    /// crosshairPosition()).
    void crosshairMoved();
    /// An axis range changed (pan, zoom, autoscale or setRange()).
    void viewChanged();
    /// canGoBack() or canGoForward() may have changed.
    void historyChanged();
    /// The context menu is about to open at @p position (widget coordinates): add to @p menu here.
    /// It is deleted once closed. For no menu, or one of your own, set the widget's
    /// contextMenuPolicy (Qt::NoContextMenu, Qt::CustomContextMenu).
    void contextMenuAboutToShow(QMenu* menu, QPointF position);
    void seriesAdded(rocketplot::Series* series);
    /// Emitted just before @p series is deleted.
    void seriesRemoved(rocketplot::Series* series);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class InteractionController;
    friend class PlotLink;
    struct Private;

    template <class SeriesType>
    SeriesType* adopt(SeriesType* series, const QString& name);
    void        seriesDataChanged();
    void        seriesStyleChanged();
    void        applyAutoscale();
    void        refitX();
    void        refitY();
    void        updateSystemTheme();
    void        setLink(PlotLink* link);
    // Repaints after a change to what the plot shows (the cached rendering is redrawn); linked
    // plots repaint too, as their margins may follow.
    void invalidate();
    void markDirty();
    // The history this plot's views are recorded in: its own, or its link's.
    [[nodiscard]] ViewHistory& history() const;
    // The pointer over the plot area, for the crosshair (nothing when it is elsewhere).
    void setHoverPosition(std::optional<QPointF> position);
    // The x of a linked plot's crosshair.
    void setLinkedCrosshair(std::optional<double> x);
    void syncCrosshair();
    void updateCursor();
    void showContextMenu(QPoint position, QPoint globalPosition);
    void renderCache();
    // The legend: what the pointer is on (as last drawn, and if the legend is interactive), what
    // that highlights, and what clicking and its menu do.
    [[nodiscard]] const LegendEntry* legendEntryAt(QPointF position) const;
    [[nodiscard]] bool               isOnLegend(QPointF position) const;
    void                             pointAt(std::optional<QPointF> position);
    [[nodiscard]] const Series*      highlightedSeries() const;
    void                             isolateSeries(Series* series);
    void                             showEntryMenu(Series* series, QPoint globalPosition);
    [[nodiscard]] QIcon              markerIcon(const Series& series, Marker shape) const;
    // Bounds of the visible series' x values (only positive ones for a log axis).
    [[nodiscard]] Range xDataBounds(bool positiveOnly) const;
    // The margins left and right of the plot area that this plot's labels need.
    [[nodiscard]] std::pair<double, double> naturalMargins() const;
    [[nodiscard]] qsizetype                 nextColorIndex();

    std::unique_ptr<Private> m_impl;
};

}  // namespace rocketplot
