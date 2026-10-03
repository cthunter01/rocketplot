#pragma once

#include <QColor>
#include <QObject>
#include <optional>

#include "rocketplot/Range.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class Axis;
class PlotWidget;
struct Theme;

/// Something a plot draws at data coordinates that isn't data: the base of ReferenceLine,
/// ShadedSpan, TextAnnotation and EventMarker. Annotations are created by the plot
/// (PlotWidget::addHorizontalLine(), addVerticalSpan(), addText(), addEvent(), ...), which owns
/// them. They move with the data as the view is panned and zoomed, and have no legend entry.
class ROCKETPLOT_EXPORT Annotation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor RESET resetColor NOTIFY changed)
    Q_PROPERTY(rocketplot::AnnotationLayer layer READ layer WRITE setLayer NOTIFY changed)
    Q_PROPERTY(
        bool onSecondaryYAxis READ isOnSecondaryYAxis WRITE setOnSecondaryYAxis NOTIFY changed)
    Q_PROPERTY(bool includedInAutoscale READ isIncludedInAutoscale WRITE setIncludedInAutoscale
                   NOTIFY changed)

public:
    ~Annotation() override;
    Q_DISABLE_COPY_MOVE(Annotation)

    /// The plot this annotation belongs to.
    [[nodiscard]] PlotWidget* plot() const noexcept { return m_plot; }

    [[nodiscard]] bool isVisible() const noexcept { return m_visible; }
    void               setVisible(bool visible);

    /// The annotation's color: the one set, or else the plot theme's for its kind (which follows
    /// theme changes, e.g. to dark mode).
    [[nodiscard]] QColor color() const;
    void                 setColor(const QColor& color);
    /// Back to the theme's color.
    void resetColor();

    /// Whether it is drawn under or over the series. Labels are always drawn over them.
    [[nodiscard]] AnnotationLayer layer() const noexcept { return m_layer; }
    void                          setLayer(AnnotationLayer layer);

    /// The y axis its y values are on (for the kinds that have them): the plot's yAxis() (the
    /// default) or yAxis2().
    [[nodiscard]] Axis* yAxis() const;
    void                setYAxis(Axis* axis);
    [[nodiscard]] bool  isOnSecondaryYAxis() const noexcept { return m_secondaryYAxis; }
    void                setOnSecondaryYAxis(bool secondary);

    /// Whether autoscale makes room for the annotation as it does for data, so that a limit line
    /// above all the data is in view. Off by default.
    [[nodiscard]] bool isIncludedInAutoscale() const noexcept { return m_includedInAutoscale; }
    void               setIncludedInAutoscale(bool included);

Q_SIGNALS:
    /// A property changed.
    void changed();

protected:
    Annotation(PlotWidget* plot, AnnotationLayer layer);

private:
    friend class PlotWidget;

    /// The theme's color for this kind of annotation.
    [[nodiscard]] virtual QColor themeColor(const Theme& theme) const;
    /// The x and y values autoscale makes room for; Range::empty() for none.
    [[nodiscard]] virtual Range xExtent() const;
    [[nodiscard]] virtual Range yExtent() const;

    PlotWidget*           m_plot;
    std::optional<QColor> m_color;
    AnnotationLayer       m_layer;
    bool                  m_visible             = true;
    bool                  m_secondaryYAxis      = false;
    bool                  m_includedInAutoscale = false;
};

}  // namespace rocketplot
