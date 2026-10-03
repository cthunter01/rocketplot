#pragma once

#include <QList>
#include <QObject>
#include <memory>
#include <optional>
#include <utility>

#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;
class ViewHistory;

/// Ties the x axes of several plots together, e.g. channels stacked above each other on a shared
/// time axis. Panning or zooming x in one plot does the same in the others; autoscale fits the data
/// of all of them; and their plot areas line up, whatever the width of each plot's y labels. The
/// crosshair of one shows its x in the others, and they share one view history, so
/// PlotWidget::back() in any of them undoes the last pan or zoom in whichever it happened.
///
/// @code
/// auto* link = new rocketplot::PlotLink(window);
/// link->addPlot(altitudePlot);
/// link->addPlot(velocityPlot);
/// @endcode
///
/// A plot belongs to at most one link. The linked x axes should have the same scale type.
class ROCKETPLOT_EXPORT PlotLink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool alignMargins READ alignsMargins WRITE setAlignMargins NOTIFY changed)
    Q_PROPERTY(bool linkCrosshair READ linksCrosshair WRITE setLinkCrosshair NOTIFY changed)

public:
    explicit PlotLink(QObject* parent = nullptr);
    ~PlotLink() override;
    Q_DISABLE_COPY_MOVE(PlotLink)

    /// Adds @p plot (taking it out of any other link); its x range becomes the link's.
    void                             addPlot(PlotWidget* plot);
    void                             removePlot(PlotWidget* plot);
    [[nodiscard]] QList<PlotWidget*> plots() const;

    /// Whether the plots' left and right margins are made equal, so their plot areas line up.
    /// On by default.
    [[nodiscard]] bool alignsMargins() const noexcept { return m_alignMargins; }
    void               setAlignMargins(bool align);

    /// Whether a plot's crosshair (PlotWidget::setCrosshairEnabled()) shows as a line at the same x
    /// in the others that have theirs on. On by default.
    [[nodiscard]] bool linksCrosshair() const noexcept { return m_linkCrosshair; }
    void               setLinkCrosshair(bool link);

Q_SIGNALS:
    void changed();

private:
    friend class PlotWidget;
    // Copies the x range and autoscale of @p source to the other plots.
    void syncFrom(const PlotWidget* source);
    // Shows the x of @p source's crosshair (nothing: hidden) in the other plots.
    void syncCrosshair(const PlotWidget* source, std::optional<double> x);
    // Redraws every plot (their margins may change).
    void updateAll();
    // Bounds of what every plot's x autoscale fits (see PlotWidget::xDataBounds()), for a shared
    // autoscale.
    [[nodiscard]] Range xDataBounds(bool positiveOnly, bool withAnnotations) const;
    // The widest left and right margins any of the plots needs.
    [[nodiscard]] std::pair<double, double> alignedMargins() const;

    QList<PlotWidget*>           m_plots;
    std::unique_ptr<ViewHistory> m_history;
    bool                         m_alignMargins  = true;
    bool                         m_linkCrosshair = true;
    bool                         m_syncing       = false;
};

}  // namespace rocketplot
