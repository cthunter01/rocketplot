#pragma once

#include <QList>
#include <QObject>
#include <QPen>
#include <Qt>
#include <optional>

#include "rocketplot/Series.h"
#include "rocketplot/export.h"

class QJsonObject;

namespace rocketplot
{

class PlotWidget;

/// A series drawn as connected lines, optionally with markers at the points (shown once zoomed in
/// far enough that the points are apart, or always: setMarkersAlwaysVisible()). Created by
/// PlotWidget::addLine().
class ROCKETPLOT_EXPORT LineSeries : public Series
{
    Q_OBJECT
    Q_PROPERTY(
        double lineWidth READ lineWidth WRITE setLineWidth RESET resetLineWidth NOTIFY changed)
    Q_PROPERTY(Qt::PenStyle lineStyle READ lineStyle WRITE setLineStyle NOTIFY changed)
    Q_PROPERTY(QList<qreal> dashPattern READ dashPattern WRITE setDashPattern NOTIFY changed)
    Q_PROPERTY(
        Qt::PenCapStyle capStyle READ capStyle WRITE setCapStyle RESET resetCapStyle NOTIFY changed)
    Q_PROPERTY(bool markersAlwaysVisible READ areMarkersAlwaysVisible WRITE setMarkersAlwaysVisible
                   NOTIFY changed)

public:
    ~LineSeries() override;
    Q_DISABLE_COPY_MOVE(LineSeries)

    /// Line width in device-independent pixels; by default the theme's.
    [[nodiscard]] double lineWidth() const;
    void                 setLineWidth(double width);
    void                 resetLineWidth();

    [[nodiscard]] Qt::PenStyle lineStyle() const noexcept { return m_lineStyle; }
    void                       setLineStyle(Qt::PenStyle style);

    /// Dashes and gaps of the line's own, in turn, in device-independent pixels whatever the line's
    /// width: {6, 4} is 6-pixel dashes 4 pixels apart. Empty (the default): lineStyle() says. A
    /// pattern of an odd count of lengths, or with one that isn't positive, is ignored with a
    /// warning. The dashes are as long as given with Qt::FlatCap; other caps add half the line's
    /// width at each end. The pattern goes on across the whole line, also where it leaves the
    /// plot area and comes back; it starts again after a gap.
    [[nodiscard]] QList<qreal> dashPattern() const { return m_dashPattern; }
    void                       setDashPattern(const QList<qreal>& pattern);

    /// The ends of the line and of its dashes: Qt::RoundCap by default.
    [[nodiscard]] Qt::PenCapStyle capStyle() const noexcept;
    void                          setCapStyle(Qt::PenCapStyle style);
    void                          resetCapStyle();

    /// Whether a marker is drawn at every point however close together they are (one per device
    /// pixel at most, which leaves out none that would show). Off by default: markers show once
    /// the points are apart.
    [[nodiscard]] bool areMarkersAlwaysVisible() const noexcept { return m_markersAlwaysVisible; }
    void               setMarkersAlwaysVisible(bool visible);

    /// The pen the line is drawn with: its color, width, style or dash pattern and cap, with round
    /// joins.
    [[nodiscard]] QPen pen() const;
    /// Sets color, width, style (or dash pattern) and cap from @p pen.
    void setPen(const QPen& pen);

protected:
    void saveState(QJsonObject& state) const override;
    void restoreState(const QJsonObject& state) override;

private:
    friend class PlotWidget;
    explicit LineSeries(PlotWidget* plot);

    std::optional<double>          m_lineWidth;
    QList<qreal>                   m_dashPattern;
    std::optional<Qt::PenCapStyle> m_capStyle;
    Qt::PenStyle                   m_lineStyle            = Qt::SolidLine;
    bool                           m_markersAlwaysVisible = false;
};

}  // namespace rocketplot
