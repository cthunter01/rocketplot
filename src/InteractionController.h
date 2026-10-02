#pragma once

#include <QPointF>
#include <Qt>
#include <cstdint>
#include <functional>
#include <vector>

#include "PlotLayout.h"
#include "core/AxisMapping.h"

class QMouseEvent;
class QWheelEvent;

namespace rocketplot
{

class Axis;
class PlotWidget;

/// Turns mouse input into view changes. Each gesture (a drag, the wheel, a double-click) with its
/// button and modifier keys is looked up in a table of bindings to find its action; where the
/// pointer is (over the plot or an axis) then decides which axes the action applies to.
///
/// Pan and zoom keep the data value under the pointer where it is, even when the change makes the
/// axes' labels wider or narrower and so moves the plot area.
class InteractionController
{
public:
    enum class Region : std::uint8_t
    {
        PLOT,
        X_AXIS,
        Y_AXIS,
        Y2_AXIS,
        OUTSIDE,
    };
    enum class Gesture : std::uint8_t
    {
        DRAG,
        WHEEL,
        DOUBLE_CLICK,
    };
    enum class Action : std::uint8_t
    {
        NONE,
        PAN,   ///< Every axis over the plot, one over an axis
        ZOOM,  ///< Every axis over the plot, one over an axis
        ZOOM_X,
        ZOOM_Y,  ///< Both y axes over the plot, one over a y axis
        RESET,   ///< Back to autoscale
    };
    /// A gesture to look up. The button is Qt::NoButton for the wheel; the modifiers must match
    /// exactly (the keypad modifier is ignored).
    struct Binding
    {
        Gesture               gesture   = Gesture::DRAG;
        Qt::MouseButton       button    = Qt::NoButton;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        Action                action    = Action::NONE;
    };

    /// @p layout computes the plot's current layout.
    InteractionController(PlotWidget& plot, std::function<PlotLayout()> layout);

    [[nodiscard]] static Region regionAt(const PlotLayout& layout, QPointF position);

    /// Each returns whether the event was used.
    bool mousePress(const QMouseEvent& event);
    bool mouseMove(const QMouseEvent& event);
    bool mouseRelease(const QMouseEvent& event);
    bool mouseDoubleClick(const QMouseEvent& event);
    bool wheel(const QWheelEvent& event);

    [[nodiscard]] const std::vector<Binding>& bindings() const noexcept { return m_bindings; }

private:
    // An axis being panned or zoomed, with the value that must stay under the pointer.
    struct Target
    {
        Axis*             axis = nullptr;
        core::AxisMapping mapping;  // when the gesture began
        double            value      = 0.0;
        bool              horizontal = true;
    };

    [[nodiscard]] Action              actionFor(Gesture gesture, Qt::MouseButton button,
                                                Qt::KeyboardModifiers modifiers) const;
    [[nodiscard]] std::vector<Target> targetsFor(Action action, Region region, QPointF position,
                                                 const PlotLayout& layout) const;
    [[nodiscard]] const AxisLayout&   layoutOf(const PlotLayout& layout, const Axis* axis) const;
    // Pans each target's axis so its value lies under @p pointer in the current layout.
    void keepUnderPointer(const std::vector<Target>& targets, QPointF pointer) const;

    PlotWidget*                 m_plot;
    std::function<PlotLayout()> m_layout;
    std::vector<Binding>        m_bindings;

    struct Drag
    {
        bool                active = false;
        QPointF             start;
        std::vector<Target> targets;
    };
    Drag m_drag;
};

}  // namespace rocketplot
