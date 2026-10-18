#pragma once

#include <QColor>
#include <QGradientStops>
#include <QObject>
#include <QRgb>
#include <QString>
#include <array>
#include <cstddef>
#include <optional>

#include "rocketplot/Range.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// The colors a plot gives values (ScatterSeries::setColorValues()), with a key: a bar of the
/// colors right of the plot area, with ticks and a label. Each plot has one,
/// PlotWidget::colorScale().
///
/// @code
/// rocketplot::ScatterSeries* landings = plot->addScatter(crossrange, downrange, "Landings");
/// landings->setColorValues(windSpeed);
/// plot->colorScale()->setLabel("Wind (m/s)");
/// plot->colorScale()->setColorMap(rocketplot::ColorMap::CIVIDIS);
/// @endcode
///
/// The scale runs from min() to max(): while it autoscales (the default) those are the smallest and
/// largest finite color values of the visible series; setRange() fixes them, and values beyond are
/// given the color of the end they are past. A NaN value has nanColor(). The key shows once a
/// visible series has color values, unless setVisible() says otherwise.
class ROCKETPLOT_EXPORT ColorScale : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible RESET resetVisible NOTIFY changed)
    Q_PROPERTY(double min READ min WRITE setMin NOTIFY changed)
    Q_PROPERTY(double max READ max WRITE setMax NOTIFY changed)
    Q_PROPERTY(bool autoscale READ autoscale WRITE setAutoscale NOTIFY changed)
    Q_PROPERTY(rocketplot::ColorMap colorMap READ colorMap WRITE setColorMap RESET resetColorMap
                   NOTIFY changed)
    Q_PROPERTY(QColor nanColor READ nanColor WRITE setNanColor RESET resetNanColor NOTIFY changed)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(rocketplot::NumberFormat numberFormat READ numberFormat WRITE setNumberFormat NOTIFY
                   changed)

public:
    ~ColorScale() override;
    Q_DISABLE_COPY_MOVE(ColorScale)

    /// Whether the key is shown: as set, or else whether a visible series has color values.
    [[nodiscard]] bool isVisible() const;
    void               setVisible(bool visible);
    void               resetVisible();

    /// The values the colors run between.
    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] double min() const noexcept { return m_range.min; }
    [[nodiscard]] double max() const noexcept { return m_range.max; }
    /// Fixes the range and turns autoscale off. Ends the wrong way round are swapped; a range that
    /// isn't finite, or too small to tell its ends apart, is ignored with a warning.
    void setRange(Range range);
    void setRange(double min, double max) { setRange(Range{.min = min, .max = max}); }
    void setMin(double min) { setRange(min, m_range.max); }
    void setMax(double max) { setRange(m_range.min, max); }

    /// Whether the range follows the color values of the visible series (on by default). Turning
    /// it on fits them at once.
    [[nodiscard]] bool autoscale() const noexcept { return m_autoscale; }
    void               setAutoscale(bool enabled);

    [[nodiscard]] ColorMap colorMap() const noexcept { return m_colorMap; }
    /// One of the built-in maps (VIRIDIS by default), or CUSTOM for the gradient last set.
    void setColorMap(ColorMap map);
    void resetColorMap() { setColorMap(ColorMap::VIRIDIS); }
    /// A map of your own: colors at positions from 0 (the minimum) to 1 (the maximum), as for a
    /// QLinearGradient. Sets the map to CUSTOM. Stops out of order are sorted; positions outside
    /// 0 to 1 are clamped; with no stops, the map stays as it is.
    void                         setGradient(const QGradientStops& stops);
    [[nodiscard]] QGradientStops gradient() const;

    /// The color of NaN values: the theme's secondary text color unless set.
    [[nodiscard]] QColor nanColor() const;
    void                 setNanColor(const QColor& color);
    void                 resetNanColor();

    /// The text beside the key, "Wind (m/s)"; empty (the default) for none. Math between $ signs
    /// and Qt rich text work, as in axis labels.
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    /// How the key writes its numbers.
    [[nodiscard]] NumberFormat numberFormat() const noexcept { return m_numberFormat; }
    void                       setNumberFormat(NumberFormat format);

    /// The color of @p value.
    [[nodiscard]] QColor colorAt(double value) const;

Q_SIGNALS:
    /// A setting changed, or the range did.
    void changed();
    void rangeChanged(double min, double max);

private:
    friend class PlotRenderer;
    friend class PlotWidget;
    explicit ColorScale(PlotWidget* plot);

    // The colors at 256 even steps from min() to max(), for coloring many values.
    using Lookup = std::array<QRgb, 256>;
    [[nodiscard]] Lookup lookup() const;
    // The range autoscale gives, which leaves autoscale on.
    void applyRange(Range range);
    void saveState(QJsonObject& state) const;
    void restoreState(const QJsonObject& state);

    PlotWidget*           m_plot;
    std::optional<bool>   m_visible;
    Range                 m_range{.min = 0.0, .max = 1.0};
    bool                  m_autoscale = true;
    ColorMap              m_colorMap  = ColorMap::VIRIDIS;
    QGradientStops        m_stops;
    std::optional<QColor> m_nanColor;
    QString               m_label;
    NumberFormat          m_numberFormat = NumberFormat::AUTO;
};

}  // namespace rocketplot
