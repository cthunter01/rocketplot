#include "InteractionController.h"

#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QWheelEvent>
#include <Qt>
#include <cmath>

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

void applyRange(Axis& axis, Range range)
{
    if (core::isUsableRange(range))
    {
        axis.setRange(range);
    }
}

}  // namespace

InteractionController::InteractionController(PlotWidget& plot)
  : m_plot(&plot),
    m_bindings{
        {.gesture   = Gesture::DRAG,
         .button    = Qt::LeftButton,
         .modifiers = Qt::NoModifier,
         .action    = Action::PAN},
        {.gesture   = Gesture::WHEEL,
         .button    = Qt::NoButton,
         .modifiers = Qt::NoModifier,
         .action    = Action::ZOOM},
        {.gesture   = Gesture::WHEEL,
         .button    = Qt::NoButton,
         .modifiers = Qt::ControlModifier,
         .action    = Action::ZOOM_X},
        {.gesture   = Gesture::WHEEL,
         .button    = Qt::NoButton,
         .modifiers = Qt::ShiftModifier,
         .action    = Action::ZOOM_Y},
        {.gesture   = Gesture::DOUBLE_CLICK,
         .button    = Qt::LeftButton,
         .modifiers = Qt::NoModifier,
         .action    = Action::RESET},
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
    if (layout.plot.contains(position))
    {
        return Region::PLOT;
    }
    // Below the plot (as wide as it) is the x axis; left of it (as tall as it) the y axis.
    if (position.x() >= layout.plot.left() && position.x() <= layout.plot.right() &&
        position.y() > layout.plot.bottom())
    {
        return Region::X_AXIS;
    }
    if (position.y() >= layout.plot.top() && position.y() <= layout.plot.bottom() &&
        position.x() < layout.plot.left())
    {
        return Region::Y_AXIS;
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

bool InteractionController::mousePress(const QMouseEvent& event, const PlotLayout& layout)
{
    const Region region = regionAt(layout, event.position());
    if (region == Region::OUTSIDE ||
        actionFor(Gesture::DRAG, event.button(), event.modifiers()) != Action::PAN)
    {
        return false;
    }
    m_drag = {.active = true,
              .start  = event.position(),
              .panX   = region != Region::Y_AXIS,
              .panY   = region != Region::X_AXIS,
              .x      = layout.x.mapping,
              .y      = layout.y.mapping};
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
    if (m_drag.panX)
    {
        applyRange(*m_plot->xAxis(), core::pannedRange(m_drag.x, delta.x()));
    }
    if (m_drag.panY)
    {
        applyRange(*m_plot->yAxis(), core::pannedRange(m_drag.y, delta.y()));
    }
    return true;
}

bool InteractionController::mouseRelease(const QMouseEvent& /*event*/)
{
    if (!m_drag.active)
    {
        return false;
    }
    m_drag.active = false;
    m_plot->unsetCursor();
    return true;
}

bool InteractionController::mouseDoubleClick(const QMouseEvent& event, const PlotLayout& layout)
{
    if (regionAt(layout, event.position()) == Region::OUTSIDE ||
        actionFor(Gesture::DOUBLE_CLICK, event.button(), event.modifiers()) != Action::RESET)
    {
        return false;
    }
    qCDebug(lcInput) << "reset view";
    m_plot->resetView();
    return true;
}

bool InteractionController::wheel(const QWheelEvent& event, const PlotLayout& layout)
{
    const Region region = regionAt(layout, event.position());
    const Action action = actionFor(Gesture::WHEEL, Qt::NoButton, event.modifiers());
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
    zoom(action, region, event.position(), std::pow(2.0, -notches * kZoomPerNotch), layout);
    return true;
}

void InteractionController::zoom(Action action, Region region, QPointF position, double factor,
                                 const PlotLayout& layout)
{
    bool zoomX = action == Action::ZOOM_X;
    bool zoomY = action == Action::ZOOM_Y;
    if (action == Action::ZOOM)
    {
        zoomX = region != Region::Y_AXIS;
        zoomY = region != Region::X_AXIS;
    }
    qCDebug(lcInput) << "zoom by" << factor << "x:" << zoomX << "y:" << zoomY;
    if (zoomX)
    {
        applyRange(*m_plot->xAxis(), core::zoomedRange(layout.x.mapping, position.x(), factor));
    }
    if (zoomY)
    {
        applyRange(*m_plot->yAxis(), core::zoomedRange(layout.y.mapping, position.y(), factor));
    }
}

}  // namespace rocketplot
