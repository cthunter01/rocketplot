#include "InteractionController.h"

#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QWheelEvent>
#include <Qt>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "Logging.h"
#include "PlotLayout.h"
#include "core/AxisMapping.h"
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"

namespace rocketplot
{

namespace
{

// One wheel notch (120 units of angleDelta) zooms in by 2^(1/4) (the view gets 16% smaller).
constexpr double kWheelUnitsPerNotch = 120.0;
constexpr double kZoomPerNotch       = 0.25;
// Modifiers that take part in bindings; others (the keypad flag) are ignored.
constexpr Qt::KeyboardModifiers kBindingModifiers =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
// The value under the pointer may drift this far (pixels) before it is corrected.
constexpr double kPointerTolerance = 0.01;

void applyRange(Axis& axis, Range range)
{
    if (core::isUsableRange(range, scaleOf(axis)))
    {
        axis.setRange(range);
    }
}

double along(QPointF point, bool horizontal)
{
    return horizontal ? point.x() : point.y();
}

}  // namespace

InteractionController::InteractionController(PlotWidget& plot, std::function<PlotLayout()> layout)
  : m_plot(&plot),
    m_layout(std::move(layout)),
    m_bindings{
        {
            .gesture   = Gesture::DRAG,
            .button    = Qt::LeftButton,
            .modifiers = Qt::NoModifier,
            .action    = Action::PAN,
        },
        {
            .gesture   = Gesture::WHEEL,
            .button    = Qt::NoButton,
            .modifiers = Qt::NoModifier,
            .action    = Action::ZOOM,
        },
        {
            .gesture   = Gesture::WHEEL,
            .button    = Qt::NoButton,
            .modifiers = Qt::ControlModifier,
            .action    = Action::ZOOM_X,
        },
        {
            .gesture   = Gesture::WHEEL,
            .button    = Qt::NoButton,
            .modifiers = Qt::ShiftModifier,
            .action    = Action::ZOOM_Y,
        },
        {
            .gesture   = Gesture::DOUBLE_CLICK,
            .button    = Qt::LeftButton,
            .modifiers = Qt::NoModifier,
            .action    = Action::RESET,
        },
    }
{
}

InteractionController::Region InteractionController::regionAt(const PlotLayout& layout,
                                                              QPointF           position)
{
    if (!layout.valid)
    {
        return Region::OUTSIDE;
    }
    const QRectF& plot = layout.plot;
    if (plot.contains(position))
    {
        return Region::PLOT;
    }
    // Below the plot (as wide as it) is the x axis; left and right of it (as tall as it) the y
    // axes.
    if (position.x() >= plot.left() && position.x() <= plot.right() && position.y() > plot.bottom())
    {
        return layout.x.shown ? Region::X_AXIS : Region::OUTSIDE;
    }
    if (position.y() >= plot.top() && position.y() <= plot.bottom())
    {
        if (position.x() < plot.left() && layout.y.shown)
        {
            return Region::Y_AXIS;
        }
        if (position.x() > plot.right() && layout.y2.shown)
        {
            return Region::Y2_AXIS;
        }
    }
    return Region::OUTSIDE;
}

InteractionController::Action InteractionController::actionFor(
    Gesture gesture, Qt::MouseButton button, Qt::KeyboardModifiers modifiers) const
{
    modifiers &= kBindingModifiers;
    for (const Binding& binding : m_bindings)
    {
        if (binding.gesture == gesture && binding.button == button &&
            binding.modifiers == modifiers)
        {
            return binding.action;
        }
    }
    return Action::NONE;
}

std::vector<InteractionController::Target> InteractionController::targetsFor(
    Action action, Region region, QPointF position, const PlotLayout& layout) const
{
    // Which axes: over an axis, just that one; over the plot, the action's axes.
    bool x  = region == Region::PLOT || region == Region::X_AXIS;
    bool y  = region == Region::PLOT || region == Region::Y_AXIS;
    bool y2 = (region == Region::PLOT && layout.y2.shown) || region == Region::Y2_AXIS;
    if (action == Action::ZOOM_X)
    {
        x  = true;
        y  = false;
        y2 = false;
    }
    else if (action == Action::ZOOM_Y)
    {
        x = false;
    }
    std::vector<Target> targets;
    const auto add = [&](bool wanted, Axis* axis, const AxisLayout& axisLayout, bool horizontal) {
        if (wanted)
        {
            targets.push_back({
                .axis       = axis,
                .mapping    = axisLayout.mapping,
                .value      = axisLayout.mapping.toValue(along(position, horizontal)),
                .horizontal = horizontal,
            });
        }
    };
    add(x, m_plot->xAxis(), layout.x, true);
    add(y, m_plot->yAxis(), layout.y, false);
    add(y2, m_plot->yAxis2(), layout.y2, false);
    return targets;
}

const AxisLayout& InteractionController::layoutOf(const PlotLayout& layout, const Axis* axis) const
{
    if (axis == m_plot->xAxis())
    {
        return layout.x;
    }
    return axis == m_plot->yAxis() ? layout.y : layout.y2;
}

void InteractionController::keepUnderPointer(const std::vector<Target>& targets,
                                             QPointF                    pointer) const
{
    const PlotLayout layout = m_layout();
    for (const Target& target : targets)
    {
        const AxisLayout& axisLayout = layoutOf(layout, target.axis);
        const double      drift =
            along(pointer, target.horizontal) - axisLayout.mapping.toPixel(target.value);
        if (std::isfinite(drift) && std::abs(drift) > kPointerTolerance)
        {
            applyRange(*target.axis, core::pannedRange(axisLayout.mapping, drift));
        }
    }
}

bool InteractionController::mousePress(const QMouseEvent& event)
{
    const PlotLayout layout = m_layout();
    const Region     region = regionAt(layout, event.position());
    if (region == Region::OUTSIDE ||
        actionFor(Gesture::DRAG, event.button(), event.modifiers()) != Action::PAN)
    {
        return false;
    }
    m_drag = {
        .active  = true,
        .start   = event.position(),
        .targets = targetsFor(Action::PAN, region, event.position(), layout),
    };
    m_plot->setCursor(Qt::ClosedHandCursor);
    qCDebug(lcInput) << "pan started in region" << static_cast<int>(region);
    return true;
}

bool InteractionController::mouseMove(const QMouseEvent& event)
{
    if (!m_drag.active)
    {
        return false;
    }
    const QPointF delta = event.position() - m_drag.start;
    for (const Target& target : m_drag.targets)
    {
        applyRange(*target.axis,
                   core::pannedRange(target.mapping, along(delta, target.horizontal)));
    }
    keepUnderPointer(m_drag.targets, event.position());
    return true;
}

bool InteractionController::mouseRelease(const QMouseEvent& /*event*/)
{
    if (!m_drag.active)
    {
        return false;
    }
    m_drag.active = false;
    m_drag.targets.clear();
    m_plot->unsetCursor();
    return true;
}

bool InteractionController::mouseDoubleClick(const QMouseEvent& event)
{
    if (regionAt(m_layout(), event.position()) == Region::OUTSIDE ||
        actionFor(Gesture::DOUBLE_CLICK, event.button(), event.modifiers()) != Action::RESET)
    {
        return false;
    }
    qCDebug(lcInput) << "reset view";
    m_plot->resetView();
    return true;
}

bool InteractionController::wheel(const QWheelEvent& event)
{
    const PlotLayout layout = m_layout();
    const Region     region = regionAt(layout, event.position());
    const Action     action = actionFor(Gesture::WHEEL, Qt::NoButton, event.modifiers());
    if (region == Region::OUTSIDE || action == Action::NONE)
    {
        return false;
    }
    // Some platforms turn Shift+wheel into horizontal scrolling; otherwise horizontal scrolling
    // isn't zoom.
    const QPoint angle = event.angleDelta();
    int          units = angle.y();
    if (units == 0 && action == Action::ZOOM_Y)
    {
        units = angle.x();
    }
    if (units == 0)
    {
        return false;
    }
    const double notches = static_cast<double>(units) / kWheelUnitsPerNotch;
    const double factor  = std::pow(2.0, -notches * kZoomPerNotch);
    const auto   targets = targetsFor(action, region, event.position(), layout);
    qCDebug(lcInput) << "zoom by" << factor << "on" << targets.size() << "axes";
    for (const Target& target : targets)
    {
        applyRange(
            *target.axis,
            core::zoomedRange(target.mapping, along(event.position(), target.horizontal), factor));
    }
    keepUnderPointer(targets, event.position());
    return true;
}

}  // namespace rocketplot
