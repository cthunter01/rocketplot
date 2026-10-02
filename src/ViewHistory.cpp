#include "ViewHistory.h"

#include <QList>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "PlotLayout.h"
#include "core/AxisMapping.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

namespace
{

// Views kept to go back to.
constexpr std::size_t kMaxViews = 50;

void restoreAxis(Axis& axis, Range range, bool autoscale)
{
    if (autoscale || !core::isUsableRange(range, scaleOf(axis)))
    {
        axis.setAutoscale(true);
    }
    else
    {
        axis.setRange(range);
    }
}

}  // namespace

ViewHistory::ViewHistory(std::function<QList<PlotWidget*>()> plots, std::function<void()> changed)
  : m_plots(std::move(plots)), m_changed(std::move(changed))
{
}

void ViewHistory::beginStep()
{
    m_pending = capture();
}

void ViewHistory::commitStep()
{
    if (!m_pending || *m_pending == capture())
    {
        return;
    }
    m_back.push_back(std::move(*m_pending));
    m_pending.reset();
    if (m_back.size() > kMaxViews)
    {
        m_back.erase(m_back.begin());
    }
    m_forward.clear();
    m_changed();
}

void ViewHistory::back()
{
    go(m_back, m_forward);
}

void ViewHistory::forward()
{
    go(m_forward, m_back);
}

void ViewHistory::clear()
{
    m_back.clear();
    m_forward.clear();
    m_pending.reset();
    m_changed();
}

void ViewHistory::go(std::vector<View>& from, std::vector<View>& to)
{
    if (from.empty())
    {
        return;
    }
    m_pending.reset();
    to.push_back(capture());
    const View view = std::move(from.back());
    from.pop_back();
    restore(view);
    m_changed();
}

ViewHistory::View ViewHistory::capture() const
{
    View       view;
    const auto axisView = [](const Axis& axis) {
        return AxisView{.range = axis.range(), .autoscale = axis.autoscale()};
    };
    for (PlotWidget* plot : m_plots())
    {
        view.push_back({
            .plot = plot,
            .x    = axisView(*plot->xAxis()),
            .y    = axisView(*plot->yAxis()),
            .y2   = axisView(*plot->yAxis2()),
        });
    }
    return view;
}

void ViewHistory::restore(const View& view)
{
    // x first: y axes that fit what's visible in x refit to it.
    for (const PlotView& plotView : view)
    {
        if (const PlotWidget* plot = plotView.plot.data())
        {
            restoreAxis(*plot->xAxis(), plotView.x.range, plotView.x.autoscale);
            restoreAxis(*plot->yAxis(), plotView.y.range, plotView.y.autoscale);
            restoreAxis(*plot->yAxis2(), plotView.y2.range, plotView.y2.autoscale);
        }
    }
}

}  // namespace rocketplot
