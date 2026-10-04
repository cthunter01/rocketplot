#include "rocketplot/PlotLink.h"

#include <QList>
#include <QMarginsF>
#include <QObject>
#include <QWidget>
#include <Qt>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ViewHistory.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

namespace
{

// Whether @p plot is on screen, or will be once its window is shown: such plots hold each other's
// margins. One that was hidden, by itself or with the page or the window it is on, takes no part.
// One whose window hasn't been shown yet does, so that plots drawn before then (QWidget::grab(),
// an export) line up as they will on screen.
bool isShownOrAboutToBe(const PlotWidget& plot)
{
    const QWidget* window = plot.window();
    if (window->isVisible())
    {
        return plot.isVisible();
    }
    // Not shown yet, rather than hidden or closed.
    return !window->testAttribute(Qt::WA_WState_ExplicitShowHide) &&
           (plot.isWindow() || plot.isVisibleTo(window));
}

}  // namespace

PlotLink::PlotLink(QObject* parent)
  : QObject(parent),
    m_history(std::make_unique<ViewHistory>([this] { return m_plots; },
                                            [this] {
                                                for (PlotWidget* plot : std::as_const(m_plots))
                                                {
                                                    Q_EMIT plot->historyChanged();
                                                }
                                            }))
{
}

PlotLink::~PlotLink()
{
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->setLink(nullptr);
    }
}

void PlotLink::addPlot(PlotWidget* plot)
{
    if (plot == nullptr || m_plots.contains(plot))
    {
        return;
    }
    if (PlotLink* previous = plot->link())
    {
        previous->removePlot(plot);
    }
    const PlotWidget* first = m_plots.isEmpty() ? nullptr : m_plots.front();
    m_plots.append(plot);
    plot->setLink(this);
    m_history->clear();  // its views don't cover the newcomer
    connect(plot, &QObject::destroyed, this, [this, plot] { m_plots.removeOne(plot); });
    connect(plot, &PlotWidget::viewChanged, this, &PlotLink::updateAll);
    if (first != nullptr)
    {
        // The newcomer takes the link's x range, then autoscale (if on) fits everyone's data.
        syncFrom(first);
        if (plot->xAxis()->autoscale())
        {
            plot->applyAutoscale();
        }
    }
    updateAll();
    Q_EMIT changed();
}

void PlotLink::removePlot(PlotWidget* plot)
{
    if (!m_plots.removeOne(plot))
    {
        return;
    }
    disconnect(plot, nullptr, this, nullptr);
    plot->setLink(nullptr);
    m_history->clear();
    updateAll();
    Q_EMIT changed();
}

QList<PlotWidget*> PlotLink::plots() const
{
    return m_plots;
}

void PlotLink::setAlignMargins(bool align)
{
    if (align == m_alignMargins)
    {
        return;
    }
    m_alignMargins = align;
    updateAll();
    Q_EMIT changed();
}

void PlotLink::setLinkCrosshair(bool link)
{
    if (link == m_linkCrosshair)
    {
        return;
    }
    m_linkCrosshair = link;
    if (!link)
    {
        for (PlotWidget* plot : std::as_const(m_plots))
        {
            plot->setLinkedCrosshair(std::nullopt);
        }
    }
    Q_EMIT changed();
}

void PlotLink::syncCrosshair(const PlotWidget* source, std::optional<double> x)
{
    if (!m_linkCrosshair)
    {
        return;
    }
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        if (plot != source)
        {
            plot->setLinkedCrosshair(x);
        }
    }
}

void PlotLink::syncFrom(const PlotWidget* source)
{
    if (m_syncing)
    {
        return;
    }
    m_syncing           = true;
    const Axis& xSource = *source->xAxis();
    for (const PlotWidget* plot : std::as_const(m_plots))
    {
        if (plot != source)
        {
            plot->xAxis()->applyAutoscale(xSource.autoscale());
            plot->xAxis()->applyRange(xSource.range());
        }
    }
    m_syncing = false;
}

void PlotLink::updateAll()
{
    for (PlotWidget* plot : std::as_const(m_plots))
    {
        plot->markDirty();
    }
}

Range PlotLink::xDataBounds(bool positiveOnly, bool withAnnotations) const
{
    Range bounds = Range::empty();
    for (const PlotWidget* plot : m_plots)
    {
        bounds = bounds.united(plot->xDataBounds(positiveOnly, withAnnotations));
    }
    return bounds;
}

std::pair<double, double> PlotLink::alignedMargins() const
{
    double left  = 0.0;
    double right = 0.0;
    for (const PlotWidget* plot : m_plots)
    {
        if (isShownOrAboutToBe(*plot) && !plot->rect().isEmpty())
        {
            const QMarginsF margins = plot->naturalMargins();
            left                    = std::max(left, margins.left());
            right                   = std::max(right, margins.right());
        }
    }
    return {left, right};
}

}  // namespace rocketplot
