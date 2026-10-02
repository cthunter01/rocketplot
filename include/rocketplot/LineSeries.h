#pragma once

#include <QObject>
#include <QPen>
#include <Qt>
#include <optional>

#include "rocketplot/Series.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// A series drawn as connected lines, optionally with markers at the points (shown once zoomed in
/// far enough that the points are apart). Created by PlotWidget::addLine().
class ROCKETPLOT_EXPORT LineSeries : public Series
{
    Q_OBJECT
    Q_PROPERTY(
        double lineWidth READ lineWidth WRITE setLineWidth RESET resetLineWidth NOTIFY changed)
    Q_PROPERTY(Qt::PenStyle lineStyle READ lineStyle WRITE setLineStyle NOTIFY changed)

public:
    ~LineSeries() override;
    Q_DISABLE_COPY_MOVE(LineSeries)

    /// Line width in device-independent pixels; by default the theme's.
    [[nodiscard]] double lineWidth() const;
    void                 setLineWidth(double width);
    void                 resetLineWidth();

    [[nodiscard]] Qt::PenStyle lineStyle() const noexcept { return m_lineStyle; }
    void                       setLineStyle(Qt::PenStyle style);

    /// The pen the line is drawn with (color, width and style, with round caps and joins).
    [[nodiscard]] QPen pen() const;
    /// Sets color, width and style from @p pen.
    void setPen(const QPen& pen);

private:
    friend class PlotWidget;
    explicit LineSeries(PlotWidget* plot);

    std::optional<double> m_lineWidth;
    Qt::PenStyle          m_lineStyle = Qt::SolidLine;
};

}  // namespace rocketplot
