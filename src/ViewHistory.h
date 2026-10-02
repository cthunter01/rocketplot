#pragma once

#include <QList>
#include <QPointer>
#include <functional>
#include <optional>
#include <vector>

#include "rocketplot/Range.h"

namespace rocketplot
{

class Axis;
class PlotWidget;

/// The views a plot, or a group of linked plots, went through by panning, zooming and resetting, to
/// go back and forward through. A view is every plot's axis ranges and autoscale settings.
///
/// A step (a drag, a burst of wheel turns, a pinch) records the view before it, once the view has
/// actually changed: beginStep() when it starts, commitStep() after each change it makes. Going to
/// a new view clears the views ahead.
class ViewHistory
{
public:
    /// @p plots lists the plots whose views are recorded; @p changed is called when canGoBack() or
    /// canGoForward() may have changed.
    ViewHistory(std::function<QList<PlotWidget*>()> plots, std::function<void()> changed);

    /// Remembers the current view as the one before a step.
    void beginStep();
    /// Records the remembered view if the view has changed since (once per step).
    void commitStep();

    [[nodiscard]] bool canGoBack() const noexcept { return !m_back.empty(); }
    [[nodiscard]] bool canGoForward() const noexcept { return !m_forward.empty(); }
    void               back();
    void               forward();
    void               clear();

private:
    struct AxisView
    {
        Range range;
        bool  autoscale = false;

        // An autoscaled axis is the same view whatever its range: the data may have grown.
        friend bool operator==(const AxisView& a, const AxisView& b)
        {
            return a.autoscale == b.autoscale && (a.autoscale || a.range == b.range);
        }
    };
    struct PlotView
    {
        QPointer<PlotWidget> plot;
        AxisView             x;
        AxisView             y;
        AxisView             y2;

        friend bool operator==(const PlotView& a, const PlotView& b)
        {
            return a.plot == b.plot && a.x == b.x && a.y == b.y && a.y2 == b.y2;
        }
    };
    using View = std::vector<PlotView>;

    [[nodiscard]] View capture() const;
    static void        restore(const View& view);
    // Goes to the newest view of @p from, saving the current one in @p to.
    void go(std::vector<View>& from, std::vector<View>& to);

    std::function<QList<PlotWidget*>()> m_plots;
    std::function<void()>               m_changed;
    std::vector<View>                   m_back;
    std::vector<View>                   m_forward;
    std::optional<View>                 m_pending;
};

}  // namespace rocketplot
