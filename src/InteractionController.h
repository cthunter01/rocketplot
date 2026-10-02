#pragma once

#include <QPointF>
#include <Qt>
#include <cstdint>
#include <vector>

#include "core/AxisMapping.h"

class QMouseEvent;
class QWheelEvent;

namespace rocketplot
{

class PlotWidget;
struct PlotLayout;

/// Turns mouse input into view changes. Each gesture (a drag, the wheel, a double-click) with its
/// button and modifier keys is looked up in a table of bindings to find its action; where the
/// pointer is (over the plot or an axis) then decides which axes the action applies to.
class InteractionController
{
public:
    enum class Region : std::uint8_t
    {
        PLOT,
        X_AXIS,
        Y_AXIS,
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
        PAN,   ///< Both axes over the plot, one over an axis
        ZOOM,  ///< Both axes over the plot, one over an axis
        ZOOM_X,
        ZOOM_Y,
        RESET,  ///< Back to autoscale
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

    explicit InteractionController(PlotWidget& plot);

    [[nodiscard]] static Region regionAt(const PlotLayout& layout, QPointF position);

    /// Each returns whether the event was used.
    bool mousePress(const QMouseEvent& event, const PlotLayout& layout);
    bool mouseMove(const QMouseEvent& event);
    bool mouseRelease(const QMouseEvent& event);
    bool mouseDoubleClick(const QMouseEvent& event, const PlotLayout& layout);
    bool wheel(const QWheelEvent& event, const PlotLayout& layout);

    [[nodiscard]] const std::vector<Binding>& bindings() const noexcept { return m_bindings; }

private:
    [[nodiscard]] Action actionFor(Gesture gesture, Qt::MouseButton button,
                                   Qt::KeyboardModifiers modifiers) const;
    void                 zoom(Action action, Region region, QPointF position, double factor,
                              const PlotLayout& layout);

    PlotWidget*          m_plot;
    std::vector<Binding> m_bindings;

    struct Drag
    {
        bool              active = false;
        QPointF           start;
        bool              panX = false;
        bool              panY = false;
        core::AxisMapping x;  // the mappings when the drag began
        core::AxisMapping y;
    };
    Drag m_drag;
};

}  // namespace rocketplot
