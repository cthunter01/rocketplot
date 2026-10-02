#include "InteractionController.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventPoint>
#include <QGuiApplication>
#include <QInputDevice>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPoint>
#include <QPointF>
#include <QPointingDevice>
#include <QRectF>
#include <QStyleHints>
#include <QTouchEvent>
#include <QWheelEvent>
#include <Qt>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "LegendLayout.h"
#include "Logging.h"
#include "PlotLayout.h"
#include "ViewHistory.h"
#include "core/AxisMapping.h"
#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/Legend.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Series.h"
#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// One wheel notch (120 units of angleDelta) zooms in by 2^(1/4) (the view gets 16% smaller).
constexpr double kWheelUnitsPerNotch = 120.0;
constexpr double kZoomPerNotch       = 0.25;
// Scrolling that reports no pixel distance pans this far per notch.
constexpr double kPixelsPerNotch = 40.0;
// The value under the pointer may drift this far (pixels) before it is corrected.
constexpr double kPointerTolerance = 0.01;
// Wheel, scroll and pinch events further apart than this (ms) are separate steps in the history.
constexpr qint64 kStepGap = 500;
// Zoom boxes (plotly's rules): no box while both sides are shorter than kMinDrag; one less tall
// than kThinRatio of its width (but at most kMinZoom) zooms only x, and the other way round only y.
constexpr double kMinDrag   = 8.0;
constexpr double kMinZoom   = 20.0;
constexpr double kThinRatio = 0.6;
// Two fingers closer than this along an axis don't zoom it: their ratio would jump about.
constexpr double kMinPinchSpan = 40.0;

// Sets a range the user panned or zoomed to (which turns autoscale off). An unchanged range is
// left alone: a horizontal swipe keeps y autoscaling.
void applyRange(Axis& axis, Range range)
{
    if (range != axis.range() && core::isUsableRange(range, scaleOf(axis)))
    {
        axis.setRange(range);
    }
}

double along(QPointF point, bool horizontal)
{
    return horizontal ? point.x() : point.y();
}

bool isZoom(PlotAction action)
{
    return action == PlotAction::ZOOM || action == PlotAction::ZOOM_X ||
           action == PlotAction::ZOOM_Y;
}

// Trackpads report scroll phases (macOS, Wayland) or say what they are (X11); a mouse wheel does
// neither. Windows reports neither for precision touchpads, which then zoom like a wheel.
bool isTrackpadScroll(const QWheelEvent& event)
{
    if (event.phase() != Qt::NoScrollPhase)
    {
        return true;
    }
    const QPointingDevice* device = event.pointingDevice();
    return device != nullptr && device->type() == QInputDevice::DeviceType::TouchPad;
}

int dragDistance()
{
    return QGuiApplication::styleHints()->startDragDistance();
}

}  // namespace

InteractionController::InteractionController(PlotWidget& plot, std::function<PlotLayout()> layout)
  : m_plot(&plot), m_layout(std::move(layout))
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

const InputBindings& InteractionController::bindings() const
{
    return m_plot->inputBindings();
}

std::vector<InteractionController::Target> InteractionController::targetsFor(
    PlotAction action, Region region, QPointF position, const PlotLayout& layout) const
{
    // Which axes: over an axis, just that one; over the plot, the action's axes.
    bool x  = region == Region::PLOT || region == Region::X_AXIS;
    bool y  = region == Region::PLOT || region == Region::Y_AXIS;
    bool y2 = (region == Region::PLOT && layout.y2.shown) || region == Region::Y2_AXIS;
    if (action == PlotAction::ZOOM_X)
    {
        x  = true;
        y  = false;
        y2 = false;
    }
    else if (action == PlotAction::ZOOM_Y)
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

void InteractionController::zoom(const std::vector<Target>& targets, QPointF position,
                                 double factor) const
{
    qCDebug(lcInput) << "zoom by" << factor << "on" << targets.size() << "axes";
    for (const Target& target : targets)
    {
        applyRange(*target.axis,
                   core::zoomedRange(target.mapping, along(position, target.horizontal), factor));
    }
    keepUnderPointer(targets, position);
}

bool InteractionController::perform(PlotAction action)
{
    switch (action)
    {
        case PlotAction::RESET:
            qCDebug(lcInput) << "reset view";
            m_plot->resetView();
            return true;
        case PlotAction::BACK:
            m_plot->back();
            return true;
        case PlotAction::FORWARD:
            m_plot->forward();
            return true;
        default:
            return false;
    }
}

void InteractionController::continueStep(Continuous kind)
{
    if (m_continuous != kind || !m_continuousTimer.isValid() ||
        m_continuousTimer.elapsed() > kStepGap)
    {
        m_plot->history().beginStep();
        m_continuous = kind;
    }
    m_continuousTimer.start();
}

// Mouse
// --------------------------------------------------------------------------------------------------------

bool InteractionController::mousePress(const QMouseEvent& event)
{
    m_dropMenu = false;
    return beginDrag(event.position(), event.button(), event.modifiers());
}

bool InteractionController::mouseMove(const QMouseEvent& event)
{
    m_plot->setHoverPosition(event.position());
    if (!m_drag.active)
    {
        m_plot->pointAt(event.position());
    }
    if (!m_drag.active)
    {
        return false;
    }
    dragTo(event.position());
    return true;
}

bool InteractionController::mouseRelease(const QMouseEvent& event)
{
    if (!m_drag.active)
    {
        return false;
    }
    if (event.button() == m_drag.button)
    {
        endDrag(event.position(), true);
    }
    return true;
}

bool InteractionController::mouseDoubleClick(const QMouseEvent& event)
{
    // On the legend: an entry's series alone (never a reset).
    if (event.button() == Qt::LeftButton && m_plot->isOnLegend(event.position()))
    {
        if (const LegendEntry* entry = m_plot->legendEntryAt(event.position()))
        {
            m_plot->isolateSeries(entry->series);
        }
        return true;
    }
    if (regionAt(m_layout(), event.position()) == Region::OUTSIDE)
    {
        return false;
    }
    return perform(bindings().action(Gesture::DOUBLE_CLICK, event.button(), event.modifiers()));
}

bool InteractionController::contextMenu(const QContextMenuEvent& event)
{
    if (event.reason() != QContextMenuEvent::Mouse)
    {
        return true;
    }
    // While the right button is down for a drag or click of its own, the release decides.
    if (m_drag.active && m_drag.button == Qt::RightButton)
    {
        return false;
    }
    return !std::exchange(m_dropMenu, false);
}

void InteractionController::leave()
{
    m_plot->setHoverPosition(std::nullopt);
}

bool InteractionController::wheel(const QWheelEvent& event)
{
    const PlotLayout layout   = m_layout();
    const QPointF    position = event.position();
    const Region     region   = regionAt(layout, position);
    const bool       trackpad = isTrackpadScroll(event);
    const PlotAction action   = bindings().action(trackpad ? Gesture::SCROLL : Gesture::WHEEL,
                                                  Qt::NoButton, event.modifiers());
    if (region == Region::OUTSIDE || (action != PlotAction::PAN && !isZoom(action)))
    {
        return false;
    }
    const Continuous kind = trackpad ? Continuous::SCROLL : Continuous::WHEEL;
    if (action == PlotAction::PAN)
    {
        // The content moves with the fingers (the platform has applied natural scrolling).
        const QPointF delta =
            event.pixelDelta().isNull()
                ? QPointF(event.angleDelta()) * (kPixelsPerNotch / kWheelUnitsPerNotch)
                : QPointF(event.pixelDelta());
        if (!delta.isNull())
        {
            continueStep(kind);
            const auto targets = targetsFor(action, region, position, layout);
            for (const Target& target : targets)
            {
                applyRange(*target.axis,
                           core::pannedRange(target.mapping, along(delta, target.horizontal)));
            }
            keepUnderPointer(targets, position + delta);
            m_plot->history().commitStep();
        }
        return true;
    }
    // Some platforms turn Shift+wheel into horizontal scrolling; otherwise horizontal scrolling
    // isn't zoom.
    const QPoint angle = event.angleDelta();
    int          units = angle.y();
    if (units == 0 && action == PlotAction::ZOOM_Y)
    {
        units = angle.x();
    }
    if (units == 0)
    {
        return false;
    }
    continueStep(kind);
    const double notches = static_cast<double>(units) / kWheelUnitsPerNotch;
    zoom(targetsFor(action, region, position, layout), position,
         std::pow(2.0, -notches * kZoomPerNotch));
    m_plot->history().commitStep();
    return true;
}

// Drags
// --------------------------------------------------------------------------------------------------------

bool InteractionController::beginDrag(QPointF position, Qt::MouseButton button,
                                      Qt::KeyboardModifiers modifiers)
{
    m_continuous = Continuous::NONE;
    if (button == Qt::LeftButton && m_plot->isOnLegend(position))
    {
        const LegendEntry* entry = m_plot->legendEntryAt(position);
        m_legendGrab             = {
            .entry = entry != nullptr ? entry->series : nullptr,
            .box   = m_plot->legendArea(),
        };
        m_drag = {
            .active    = true,
            .legend    = true,
            .button    = button,
            .modifiers = modifiers,
            .start     = position,
            .current   = position,
            .plot      = m_plot->plotArea(),
            .targets   = {},
        };
        return true;
    }
    const PlotLayout layout = m_layout();
    const Region     region = regionAt(layout, position);
    PlotAction       action = bindings().action(Gesture::DRAG, button, modifiers);
    if (action != PlotAction::PAN && action != PlotAction::BOX_ZOOM)
    {
        action = PlotAction::NONE;
    }
    // A button that only clicks is still followed to its release.
    if (region == Region::OUTSIDE ||
        (action == PlotAction::NONE &&
         bindings().action(Gesture::CLICK, button, modifiers) == PlotAction::NONE))
    {
        return false;
    }
    m_continuous = Continuous::NONE;
    m_drag       = {
        .active    = true,
        .action    = action,
        .button    = button,
        .modifiers = modifiers,
        .region    = region,
        .start     = position,
        .current   = position,
        .plot      = layout.plot,
        .targets   = targetsFor(action, region, position, layout),
    };
    if (action != PlotAction::NONE)
    {
        m_plot->history().beginStep();
        qCDebug(lcInput) << action << "started in region" << static_cast<int>(region);
    }
    m_plot->updateCursor();
    return true;
}

void InteractionController::dragTo(QPointF position)
{
    m_drag.current = position;
    m_drag.moved   = m_drag.moved || (position - m_drag.start).manhattanLength() >= dragDistance();
    if (m_drag.legend)
    {
        if (m_drag.moved)
        {
            // Wherever it is dropped, it stays inside the plot area.
            const QRectF& box     = m_legendGrab.box;
            const QPointF topLeft = box.topLeft() + (position - m_drag.start);
            m_plot->legend()->setPosition(legendPosition(m_drag.plot, box.size(), topLeft));
            m_plot->updateCursor();
        }
        return;
    }
    if (m_drag.action == PlotAction::PAN)
    {
        const QPointF delta = position - m_drag.start;
        for (const Target& target : m_drag.targets)
        {
            applyRange(*target.axis,
                       core::pannedRange(target.mapping, along(delta, target.horizontal)));
        }
        keepUnderPointer(m_drag.targets, position);
        m_plot->history().commitStep();
    }
    else if (m_drag.action == PlotAction::BOX_ZOOM)
    {
        m_plot->update();
    }
}

void InteractionController::endDrag(QPointF position, bool click)
{
    dragTo(position);
    if (m_drag.legend)
    {
        const Drag drag = std::exchange(m_drag, Drag{});
        if (Series* entry = m_legendGrab.entry.data(); entry != nullptr && click && !drag.moved)
        {
            entry->setVisible(!entry->isVisible());
        }
        m_plot->pointAt(position);
        return;
    }
    if (m_drag.action == PlotAction::BOX_ZOOM)
    {
        applyBoxZoom();
    }
    if (m_drag.action != PlotAction::NONE)
    {
        m_plot->history().commitStep();
    }
    const Drag drag = std::exchange(m_drag, Drag{});
    m_plot->update();  // without the zoom box
    m_plot->updateCursor();
    const bool       clicked = click && !drag.moved;
    const PlotAction clickAction =
        clicked ? bindings().action(Gesture::CLICK, drag.button, drag.modifiers) : PlotAction::NONE;
    if (drag.button == Qt::RightButton)
    {
        // The right button was ours from the press, so the context menu is decided here: a plain
        // click opens it (through the widget, which applies its contextMenuPolicy), and the menu
        // some platforms ask for on the release is dropped.
        if (clicked && clickAction == PlotAction::NONE)
        {
            const QPoint      point = position.toPoint();
            QContextMenuEvent menu(QContextMenuEvent::Mouse, point, m_plot->mapToGlobal(point),
                                   drag.modifiers);
            QCoreApplication::sendEvent(m_plot, &menu);
        }
        m_dropMenu = true;
    }
    perform(clickAction);
}

PlotAction InteractionController::dragAction() const noexcept
{
    return m_drag.active ? m_drag.action : PlotAction::NONE;
}

bool InteractionController::isDraggingLegend() const noexcept
{
    return m_drag.active && m_drag.legend && m_drag.moved;
}

InteractionController::BoxAxes InteractionController::boxAxes() const
{
    const double width  = std::abs(m_drag.current.x() - m_drag.start.x());
    const double height = std::abs(m_drag.current.y() - m_drag.start.y());
    switch (m_drag.region)
    {
        case Region::X_AXIS:
            return width >= kMinDrag ? BoxAxes::X : BoxAxes::NONE;
        case Region::Y_AXIS:
        case Region::Y2_AXIS:
            return height >= kMinDrag ? BoxAxes::Y : BoxAxes::NONE;
        case Region::PLOT:
        case Region::OUTSIDE:
            break;
    }
    if (width < kMinDrag && height < kMinDrag)
    {
        return BoxAxes::NONE;
    }
    if (height < std::min(std::max(kThinRatio * width, kMinDrag), kMinZoom))
    {
        return BoxAxes::X;
    }
    if (width < std::min(std::max(kThinRatio * height, kMinDrag), kMinZoom))
    {
        return BoxAxes::Y;
    }
    return BoxAxes::BOTH;
}

std::optional<QRectF> InteractionController::zoomBox() const
{
    if (!m_drag.active || m_drag.action != PlotAction::BOX_ZOOM)
    {
        return std::nullopt;
    }
    const QRectF& plot  = m_drag.plot;
    const auto    clamp = [&](QPointF point) {
        return QPointF(std::clamp(point.x(), plot.left(), plot.right()),
                       std::clamp(point.y(), plot.top(), plot.bottom()));
    };
    const QRectF box = QRectF(clamp(m_drag.start), clamp(m_drag.current)).normalized();
    switch (boxAxes())
    {
        case BoxAxes::NONE:
            return std::nullopt;
        case BoxAxes::X:
            return QRectF(box.left(), plot.top(), box.width(), plot.height());
        case BoxAxes::Y:
            return QRectF(plot.left(), box.top(), plot.width(), box.height());
        case BoxAxes::BOTH:
            return box;
    }
    return std::nullopt;
}

void InteractionController::applyBoxZoom()
{
    const std::optional<QRectF> box = zoomBox();
    if (!box)
    {
        return;
    }
    const BoxAxes axes = boxAxes();
    // The layout now: live data may have scrolled under the box.
    const PlotLayout layout = m_layout();
    for (const Target& target :
         targetsFor(PlotAction::BOX_ZOOM, m_drag.region, m_drag.start, layout))
    {
        const core::AxisMapping& mapping = layoutOf(layout, target.axis).mapping;
        if (target.horizontal && axes != BoxAxes::Y)
        {
            applyRange(*target.axis, core::rangeBetween(mapping, box->left(), box->right()));
        }
        else if (!target.horizontal && axes != BoxAxes::X)
        {
            applyRange(*target.axis, core::rangeBetween(mapping, box->top(), box->bottom()));
        }
    }
    qCDebug(lcInput) << "box zoom to" << *box;
}

// Touch and gestures
// ---------------------------------------------------------------------------------------------

bool InteractionController::touch(const QTouchEvent& event)
{
    // Trackpads (macOS sends their touches too) scroll and pinch through wheel and gesture events.
    const QInputDevice* device = event.device();
    if (device == nullptr || device->type() != QInputDevice::DeviceType::TouchScreen)
    {
        return false;
    }
    std::vector<QPointF> down;  // the fingers still touching
    for (const QEventPoint& point : event.points())
    {
        if (point.state() != QEventPoint::State::Released)
        {
            down.push_back(point.position());
        }
    }
    switch (event.type())
    {
        case QEvent::TouchBegin:
            return touchBegin(down, event.modifiers());
        case QEvent::TouchUpdate:
            touchUpdate(down, event.modifiers());
            return true;
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            touchEnd(event.points().isEmpty() ? m_drag.current : event.points().front().position(),
                     event.type() == QEvent::TouchEnd);
            return true;
        default:
            return false;
    }
}

bool InteractionController::touchBegin(const std::vector<QPointF>& down,
                                       Qt::KeyboardModifiers       modifiers)
{
    if (down.empty() || regionAt(m_layout(), down.front()) == Region::OUTSIDE)
    {
        return false;
    }
    m_touch.moved = false;
    m_touch.pressed.start();
    if (down.size() >= 2)
    {
        beginPinch(down[0], down[1], modifiers);
    }
    else
    {
        beginDrag(down.front(), Qt::LeftButton, modifiers);
    }
    return true;
}

void InteractionController::touchUpdate(const std::vector<QPointF>& down,
                                        Qt::KeyboardModifiers       modifiers)
{
    if (down.size() >= 2)
    {
        m_touch.moved = true;
        if (m_pinch.active)
        {
            pinchTo(down[0], down[1]);
            return;
        }
        if (m_drag.active)
        {
            endDrag(m_drag.current, false);
        }
        beginPinch(down[0], down[1], modifiers);
    }
    else if (down.size() == 1)
    {
        if (m_pinch.active)
        {
            // One finger lifted: the other pans on.
            endPinch();
            beginDrag(down.front(), Qt::LeftButton, modifiers);
        }
        else if (m_drag.active)
        {
            dragTo(down.front());
            m_touch.moved = m_touch.moved || m_drag.moved;
        }
    }
}

void InteractionController::touchEnd(QPointF position, bool lifted)
{
    endPinch();
    if (!m_drag.active)
    {
        return;
    }
    const bool tap = lifted && !m_touch.moved;
    endDrag(position, tap);
    if (tap)
    {
        tapped(position);
    }
}

void InteractionController::tapped(QPointF position)
{
    const QStyleHints* hints    = QGuiApplication::styleHints();
    const int          interval = hints->mouseDoubleClickInterval();
    if (m_touch.pressed.elapsed() > interval)
    {
        m_touch.lastTap.invalidate();  // a long press, not a tap
        return;
    }
    if (m_touch.lastTap.isValid() && m_touch.lastTap.elapsed() <= interval &&
        (position - m_touch.lastTapPosition).manhattanLength() <= hints->touchDoubleTapDistance())
    {
        m_touch.lastTap.invalidate();
        if (m_plot->isOnLegend(position))
        {
            if (const LegendEntry* entry = m_plot->legendEntryAt(position))
            {
                m_plot->isolateSeries(entry->series);
            }
            return;
        }
        perform(bindings().action(Gesture::DOUBLE_CLICK, Qt::LeftButton, Qt::NoModifier));
        return;
    }
    m_touch.lastTap.start();
    m_touch.lastTapPosition = position;
}

void InteractionController::beginPinch(QPointF first, QPointF second,
                                       Qt::KeyboardModifiers modifiers)
{
    const PlotLayout layout = m_layout();
    const QPointF    center = (first + second) / 2.0;
    Region           region = regionAt(layout, center);
    if (region == Region::OUTSIDE)
    {
        region = Region::PLOT;
    }
    const PlotAction action = bindings().action(Gesture::PINCH, Qt::NoButton, modifiers);
    m_continuous            = Continuous::NONE;
    m_pinch                 = {
        .active = true,
        .first  = first,
        .second = second,
        .targets =
            isZoom(action) ? targetsFor(action, region, center, layout) : std::vector<Target>{},
    };
    m_plot->history().beginStep();
}

void InteractionController::pinchTo(QPointF first, QPointF second)
{
    const QPointF from = (m_pinch.first + m_pinch.second) / 2.0;
    const QPointF to   = (first + second) / 2.0;
    for (const Target& target : m_pinch.targets)
    {
        // Each axis zooms by how far the fingers spread along it, and moves with their center.
        const double before = std::abs(along(m_pinch.first - m_pinch.second, target.horizontal));
        const double after  = std::abs(along(first - second, target.horizontal));
        const double scale  = before >= kMinPinchSpan ? std::max(after, 1.0) / before : 1.0;
        applyRange(*target.axis,
                   core::transformedRange(target.mapping, along(from, target.horizontal),
                                          along(to, target.horizontal), scale));
    }
    keepUnderPointer(m_pinch.targets, to);
    m_plot->history().commitStep();
}

void InteractionController::endPinch()
{
    if (!m_pinch.active)
    {
        return;
    }
    m_plot->history().commitStep();
    m_pinch = {};
}

bool InteractionController::nativeGesture(const QNativeGestureEvent& event)
{
    switch (event.gestureType())
    {
        case Qt::ZoomNativeGesture:
        {
            const PlotLayout layout = m_layout();
            const Region     region = regionAt(layout, event.position());
            const PlotAction action =
                bindings().action(Gesture::PINCH, Qt::NoButton, event.modifiers());
            const double magnification = 1.0 + event.value();  // value: the change, + is bigger
            if (region == Region::OUTSIDE || !isZoom(action) || !(magnification > 0.0))
            {
                return false;
            }
            continueStep(Continuous::PINCH);
            zoom(targetsFor(action, region, event.position(), layout), event.position(),
                 1.0 / magnification);
            m_plot->history().commitStep();
            return true;
        }
        case Qt::BeginNativeGesture:
            // Taken where a pinch would zoom, so the gesture's events come here.
            return regionAt(m_layout(), event.position()) != Region::OUTSIDE &&
                   isZoom(bindings().action(Gesture::PINCH, Qt::NoButton, event.modifiers()));
        case Qt::EndNativeGesture:
            if (m_continuous == Continuous::PINCH)
            {
                m_continuous = Continuous::NONE;
            }
            return false;
        default:
            return false;
    }
}

}  // namespace rocketplot
