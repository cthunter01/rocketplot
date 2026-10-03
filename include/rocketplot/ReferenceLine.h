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

/// A straight line across the whole plot at one value: a limit, a target, a mean. Created by
/// PlotWidget::addHorizontalLine() (at a y value, on yAxis() unless setYAxis() says otherwise) or
/// PlotWidget::addVerticalLine() (at an x value). Dashed and drawn over the series by default.
///
/// @code
/// auto* limit = plot->addHorizontalLine(4.5, "Structural limit");
/// limit->setColor(Qt::red);
/// limit->setIncludedInAutoscale(true);   // stay in view even when the data is far below
/// @endcode
class ROCKETPLOT_EXPORT ReferenceLine : public Annotation
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation CONSTANT)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY changed)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(
        double lineWidth READ lineWidth WRITE setLineWidth RESET resetLineWidth NOTIFY changed)
    Q_PROPERTY(Qt::PenStyle lineStyle READ lineStyle WRITE setLineStyle NOTIFY changed)
    Q_PROPERTY(
        Qt::Alignment labelAlignment READ labelAlignment WRITE setLabelAlignment NOTIFY changed)

public:
    ~ReferenceLine() override;
    Q_DISABLE_COPY_MOVE(ReferenceLine)

    /// Qt::Horizontal for a line at a y value, Qt::Vertical for one at an x value.
    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }

    /// The y (horizontal line) or x (vertical line) the line is at. A value the axis can't show
    /// (NaN, or <= 0 on a log axis) draws nothing.
    [[nodiscard]] double value() const noexcept { return m_value; }
    void                 setValue(double value);

    /// Text drawn beside the line. Qt rich text works too.
    [[nodiscard]] QString label() const { return m_label; }
    void                  setLabel(const QString& label);

    /// Line width in device-independent pixels; by default the theme's.
    [[nodiscard]] double lineWidth() const;
    void                 setLineWidth(double width);
    void                 resetLineWidth();

    /// Qt::DashLine by default.
    [[nodiscard]] Qt::PenStyle lineStyle() const noexcept { return m_lineStyle; }
    void                       setLineStyle(Qt::PenStyle style);

    /// Where the label sits: at which end of the line, and on which side of it. For a horizontal
    /// line, Qt::AlignLeft, Qt::AlignHCenter or Qt::AlignRight along it, and Qt::AlignTop (above)
    /// or Qt::AlignBottom (below); for a vertical one, Qt::AlignTop, Qt::AlignVCenter or
    /// Qt::AlignBottom along it, and Qt::AlignLeft or Qt::AlignRight of it. The default,
    /// Qt::AlignRight | Qt::AlignTop, is above the right end of a horizontal line and right of the
    /// top of a vertical one. The label changes side where it wouldn't fit in the plot.
    [[nodiscard]] Qt::Alignment labelAlignment() const noexcept { return m_labelAlignment; }
    void                        setLabelAlignment(Qt::Alignment alignment);

private:
    friend class PlotWidget;
    ReferenceLine(PlotWidget* plot, Qt::Orientation orientation, double value);

    [[nodiscard]] Range xExtent() const override;
    [[nodiscard]] Range yExtent() const override;

    Qt::Orientation       m_orientation;
    double                m_value;
    QString               m_label;
    std::optional<double> m_lineWidth;
    Qt::PenStyle          m_lineStyle      = Qt::DashLine;
    Qt::Alignment         m_labelAlignment = Qt::AlignRight | Qt::AlignTop;
};

}  // namespace rocketplot
