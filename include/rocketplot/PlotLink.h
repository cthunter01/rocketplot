#pragma once

#include <QList>
#include <QObject>
#include <utility>

#include "rocketplot/Range.h"
#include "rocketplot/export.h"

namespace rocketplot
{

class PlotWidget;

/// Ties the x axes of several plots together, e.g. channels stacked above each other on a shared
/// time axis. Panning or zooming x in one plot does the same in the others; autoscale fits the data
/// of all of them; and their plot areas line up, whatever the width of each plot's y labels.
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

Q_SIGNALS:
    void changed();

private:
    friend class PlotWidget;
    // Copies the x range and autoscale of @p source to the other plots.
    void syncFrom(const PlotWidget* source);
    void updateAll();
    // Bounds of every plot's x data, for a shared autoscale.
    [[nodiscard]] Range xDataBounds(bool positiveOnly) const;
    // The widest left and right margins any of the plots needs.
    [[nodiscard]] std::pair<double, double> alignedMargins() const;

    QList<PlotWidget*> m_plots;
    bool               m_alignMargins = true;
    bool               m_syncing      = false;
};

}  // namespace rocketplot
