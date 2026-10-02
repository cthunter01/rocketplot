#pragma once

#include <QObject>
#include <QPointF>
#include <optional>

#include "rocketplot/Series.h"
#include "rocketplot/enums.h"
#include "rocketplot/export.h"

class QMenu;

namespace rocketplot
{

/// The plot's legend: one entry per named series (series with an empty name are left out). Owned by
/// its PlotWidget (PlotWidget::legend()).
///
/// By default the legend appears once there are two or more entries; a single series is better
/// named by the plot's title. setVisible() overrides that either way. It goes where it covers the
/// least data (LegendAnchor::BEST), unless anchored elsewhere or dragged.
///
/// The user can click an entry to hide or show its series (autoscale fits only what is shown),
/// double-click one to show only that series (again: all of them), point at one to bring its
/// series forward, right-click one for a menu (visibility, color, line width, marker, remove), and
/// drag the legend anywhere in the plot area. While the plot's crosshair is shown, each entry also
/// shows its series' value there.
class ROCKETPLOT_EXPORT Legend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible RESET resetVisible NOTIFY changed)
    Q_PROPERTY(rocketplot::LegendAnchor anchor READ anchor WRITE setAnchor NOTIFY changed)
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY changed)
    Q_PROPERTY(bool valuesVisible READ areValuesVisible WRITE setValuesVisible NOTIFY changed)

public:
    ~Legend() override;
    Q_DISABLE_COPY_MOVE(Legend)

    /// Whether the legend may be shown: by default true, but only drawn for two or more entries.
    [[nodiscard]] bool isVisible() const noexcept { return m_visible.value_or(true); }
    /// Shows the legend (even for a single entry) or hides it.
    void setVisible(bool visible);
    /// Back to the default: shown for two or more entries.
    void resetVisible();
    /// Whether the legend is drawn for @p entryCount entries.
    [[nodiscard]] bool isShownFor(qsizetype entryCount) const noexcept;

    /// Where the legend goes. BEST by default; dragging it makes it CUSTOM.
    [[nodiscard]] LegendAnchor anchor() const noexcept { return m_anchor; }
    void                       setAnchor(LegendAnchor anchor);

    /// Where a CUSTOM legend is, as fractions of the room the plot area leaves around it: (0, 0)
    /// puts it in the top-left corner, (1, 1) in the bottom-right, so it stays inside as the plot
    /// is resized. Setting it makes the anchor CUSTOM.
    [[nodiscard]] QPointF position() const noexcept { return m_position; }
    void                  setPosition(QPointF position);

    /// Whether the user can click, point at, right-click and drag the legend (see above). On by
    /// default.
    [[nodiscard]] bool isInteractive() const noexcept { return m_interactive; }
    void               setInteractive(bool interactive);

    /// Whether each entry shows its series' value at the crosshair
    /// (PlotWidget::setCrosshairEnabled()): the y of the point nearest the crosshair's x
    /// (Series::nearestIndex()). Series that aren't sorted by x show none. On by default.
    [[nodiscard]] bool areValuesVisible() const noexcept { return m_valuesVisible; }
    void               setValuesVisible(bool visible);

Q_SIGNALS:
    void changed();
    /// The context menu of @p series' entry is about to open: add to @p menu here. It is deleted
    /// once closed.
    void entryMenuAboutToShow(QMenu* menu, rocketplot::Series* series);

private:
    friend class PlotWidget;
    explicit Legend(QObject* parent);

    std::optional<bool> m_visible;
    LegendAnchor        m_anchor = LegendAnchor::BEST;
    QPointF             m_position{1.0, 0.0};
    bool                m_interactive   = true;
    bool                m_valuesVisible = true;
};

}  // namespace rocketplot
