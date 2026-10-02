#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <memory>
#include <span>
#include <vector>

#include "rocketplot/NumericRange.h"
#include "rocketplot/Theme.h"
#include "rocketplot/UniformX.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace rocketplot
{

class Axis;
class Legend;
class LineSeries;
class ScatterSeries;
class Series;

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
/// Drag to pan and use the wheel to zoom about the pointer; over an axis, either only affects that
/// axis, and with Ctrl (x) or Shift (y) held the wheel zooms one axis. A double-click returns to
/// autoscale.
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

    [[nodiscard]] Axis*   xAxis() const noexcept;
    [[nodiscard]] Axis*   yAxis() const noexcept;
    [[nodiscard]] Legend* legend() const noexcept;

    [[nodiscard]] QString title() const;
    void                  setTitle(const QString& title);

    /// Turns autoscale back on for both axes, fitting all visible data.
    void resetView();

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
    /// The data coordinates at a widget position, and back.
    [[nodiscard]] QPointF mapToData(QPointF widgetPosition) const;
    [[nodiscard]] QPointF mapFromData(QPointF dataPosition) const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void titleChanged();
    void themeChanged();
    void debugOverlayChanged();
    /// An axis range changed (pan, zoom, autoscale or setRange()).
    void viewChanged();
    void seriesAdded(rocketplot::Series* series);
    /// Emitted just before @p series is deleted.
    void seriesRemoved(rocketplot::Series* series);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    struct Private;

    template <class SeriesType>
    SeriesType*             adopt(SeriesType* series, const QString& name);
    void                    seriesDataChanged();
    void                    seriesStyleChanged();
    void                    applyAutoscale();
    void                    updateSystemTheme();
    [[nodiscard]] qsizetype nextColorIndex();

    std::unique_ptr<Private> m_impl;
};

}  // namespace rocketplot
