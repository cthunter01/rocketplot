#include "rocketplot/PlotLink.h"

#include <QList>
#include <QObject>
#include <algorithm>
#include <utility>

#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

PlotLink::PlotLink(QObject* parent) : QObject(parent) { }

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
        plot->update();
    }
}

Range PlotLink::xDataBounds(bool positiveOnly) const
{
    Range bounds = Range::empty();
    for (const PlotWidget* plot : m_plots)
    {
        bounds = bounds.united(plot->xDataBounds(positiveOnly));
    }
    return bounds;
}

std::pair<double, double> PlotLink::alignedMargins() const
{
    double left  = 0.0;
    double right = 0.0;
    for (const PlotWidget* plot : m_plots)
    {
        if (plot->isVisible() && !plot->rect().isEmpty())
        {
            const auto [plotLeft, plotRight] = plot->naturalMargins();
            left                             = std::max(left, plotLeft);
            right                            = std::max(right, plotRight);
        }
    }
    return {left, right};
}

}  // namespace rocketplot
