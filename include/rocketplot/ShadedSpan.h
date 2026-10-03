#pragma once

#include <QObject>
#include <QString>
#include <Qt>
#include <optional>

#include "rocketplot/Annotation.h"
#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// A shaded region across the whole plot between two values: a phase of flight, an allowed band, a
/// time of interest. Created by PlotWidget::addVerticalSpan() (between two x values) or
/// PlotWidget::addHorizontalSpan() (between two y values, on yAxis() unless setYAxis() says
/// otherwise). Filled in its color at opacity(), under the series by default.
///
/// An end may be infinite: the span then runs to the edge of the plot.
class ROCKETPLOT_EXPORT ShadedSpan : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation CONSTANT)
    Q_PROPERTY(double min READ min WRITE setMin NOTIFY changed)
    Q_PROPERTY(double max READ max WRITE setMax NOTIFY changed)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity RESET resetOpacity NOTIFY changed)
    Q_PROPERTY(
        Qt::Alignment labelAlignment READ labelAlignment WRITE setLabelAlignment NOTIFY changed)

public:
    ~ShadedSpan() override;
    Q_DISABLE_COPY_MOVE(ShadedSpan)

    /// Qt::Vertical for a span between two x values (it runs from the bottom of the plot to the
    /// top), Qt::Horizontal for one between two y values.
    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }

    /// The x (vertical span) or y (horizontal span) values it lies between.
    [[nodiscard]] Range  range() const noexcept { return m_range; }
    [[nodiscard]] double min() const noexcept { return m_range.min; }
    [[nodiscard]] double max() const noexcept { return m_range.max; }
    /// The ends are swapped if reversed. A span with a NaN end draws nothing.
    void setRange(Range range);
    void setRange(double min, double max) { setRange(Range{.min = min, .max = max}); }
    void setMin(double min) { setRange(min, m_range.max); }
    void setMax(double max) { setRange(m_range.min, max); }

    /// Text drawn inside the span. Qt rich text works too.
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    /// How opaque the fill is, from 0 to 1 (on top of the color's own alpha); by default the
    /// theme's.
    [[nodiscard]] double opacity() const;
    void                 setOpacity(double opacity);
    void                 resetOpacity();

    /// Where in the span (the part of it in view) the label sits: Qt::AlignLeft | Qt::AlignBottom
    /// by default, clear of the flags that events put along the top.
    [[nodiscard]] Qt::Alignment labelAlignment() const noexcept { return m_labelAlignment; }
    void                        setLabelAlignment(Qt::Alignment alignment);

private:
    friend class PlotWidget;
    ShadedSpan(PlotWidget* plot, Qt::Orientation orientation, Range range);

    [[nodiscard]] Range xExtent() const override;
    [[nodiscard]] Range yExtent() const override;

    Qt::Orientation       m_orientation;
    Range                 m_range;
    QString               m_label;
    std::optional<double> m_opacity;
    Qt::Alignment         m_labelAlignment = Qt::AlignLeft | Qt::AlignBottom;
};

}  // namespace rocketplot
