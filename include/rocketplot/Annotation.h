#pragma once

#include <QColor>
#include <QObject>
#include <QString>
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
/// them. They move with the data as the view is panned and zoomed (unless pinned to the plot area:
/// setCoordinates()), and have no legend entry unless given a legend name (setLegendName()).
class ROCKETPLOT_EXPORT Annotation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor RESET resetColor NOTIFY changed)
    Q_PROPERTY(rocketplot::AnnotationLayer layer READ layer WRITE setLayer NOTIFY changed)
    Q_PROPERTY(rocketplot::AnnotationCoordinates coordinates READ coordinates WRITE setCoordinates
                   NOTIFY changed)
    Q_PROPERTY(
        bool onSecondaryYAxis READ isOnSecondaryYAxis WRITE setOnSecondaryYAxis NOTIFY changed)
    Q_PROPERTY(bool includedInAutoscale READ isIncludedInAutoscale WRITE setIncludedInAutoscale
                   NOTIFY changed)
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip NOTIFY changed)
    Q_PROPERTY(QString legendName READ legendName WRITE setLegendName NOTIFY changed)

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

    /// What its x and y values are: data values (DATA, the default), or fractions of the plot
    /// area (PLOT_AREA), which pin it to a place in the plot whatever the view: a note in a corner,
    /// a line at a fixed height. The pixel offset of a TextAnnotation applies either way, and
    /// autoscale never makes room for a pinned annotation.
    ///
    /// @code
    /// auto* note = plot->addText(0.02, 0.98, "Simulated");
    /// note->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    /// note->setAlignment(Qt::AlignLeft | Qt::AlignTop);   // its top-left corner in the plot's
    /// @endcode
    [[nodiscard]] AnnotationCoordinates coordinates() const noexcept { return m_coordinates; }
    void                                setCoordinates(AnnotationCoordinates coordinates);

    /// The y axis its y values are on (for the kinds that have them): the plot's yAxis() (the
    /// default) or yAxis2().
    [[nodiscard]] Axis* yAxis() const;
    void                setYAxis(Axis* axis);
    [[nodiscard]] bool  isOnSecondaryYAxis() const noexcept { return m_secondaryYAxis; }
    void                setOnSecondaryYAxis(bool secondary);

    /// Whether autoscale makes room for the annotation as it does for data, so that a limit line
    /// above all the data is in view. Off by default; ignored for one pinned to the plot area.
    [[nodiscard]] bool isIncludedInAutoscale() const noexcept { return m_includedInAutoscale; }
    void               setIncludedInAutoscale(bool included);

    /// Text shown in a tool tip while the pointer rests on what the annotation draws that the
    /// pointer can be on: a text's box, an icon, an event's flag, the label of a line, a span or a
    /// rectangle (see PlotWidget::annotationAt()). Rich text works, as in any QToolTip. Empty
    /// (the default) for none.
    [[nodiscard]] QString toolTip() const { return m_toolTip; }
    void                  setToolTip(const QString& toolTip);

    /// The name of the annotation's entry in the legend, after the series' entries: for a value
    /// line ("Nominal", "Mean") or a region that the legend should explain. The entry shows a
    /// sample of the annotation; clicking it hides or shows the annotation. Empty (the default):
    /// no entry.
    [[nodiscard]] QString legendName() const { return m_legendName; }
    void                  setLegendName(const QString& name);

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
    QString               m_toolTip;
    QString               m_legendName;
    AnnotationLayer       m_layer;
    AnnotationCoordinates m_coordinates         = AnnotationCoordinates::DATA;
    bool                  m_visible             = true;
    bool                  m_secondaryYAxis      = false;
    bool                  m_includedInAutoscale = false;
};

}  // namespace rocketplot
