#pragma once

#include <QObject>
#include <optional>

#include "rocketplot/enums.h"
#include "rocketplot/export.h"

namespace rocketplot
{

/// The plot's legend: one entry per named series (series with an empty name are left out). Owned by
/// its PlotWidget (PlotWidget::legend()).
///
/// By default the legend appears once there are two or more entries; a single series is better
/// named by the plot's title. setVisible() overrides that either way.
class ROCKETPLOT_EXPORT Legend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible RESET resetVisible NOTIFY changed)
    Q_PROPERTY(rocketplot::LegendAnchor anchor READ anchor WRITE setAnchor NOTIFY changed)

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

    [[nodiscard]] LegendAnchor anchor() const noexcept { return m_anchor; }
    void                       setAnchor(LegendAnchor anchor);

Q_SIGNALS:
    void changed();

private:
    friend class PlotWidget;
    explicit Legend(QObject* parent);

    std::optional<bool> m_visible;
    LegendAnchor        m_anchor = LegendAnchor::TOP_RIGHT;
};

}  // namespace rocketplot
