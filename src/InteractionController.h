#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSizeF>
#include <Qt>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "PlotLayout.h"
#include "core/AxisMapping.h"
#include "rocketplot/enums.h"

class QContextMenuEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QTouchEvent;
class QWheelEvent;

namespace rocketplot
{

class Axis;
class InputBindings;
class PlotWidget;
class Series;

/// Turns mouse, trackpad and touch input into view changes. Each gesture with its button and
/// modifier keys is looked up in the plot's InputBindings to find its action; where the pointer is
/// (over the plot or an axis) then decides which axes the action applies to. Each pan, zoom or
/// reset is a step in the plot's view history.
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

    /// @p layout computes the plot's current layout.
    InteractionController(PlotWidget& plot, std::function<PlotLayout()> layout);

    [[nodiscard]] static Region regionAt(const PlotLayout& layout, QPointF position);

    /// Each returns whether the event was used.
    bool mousePress(const QMouseEvent& event);
    bool mouseMove(const QMouseEvent& event);
    bool mouseRelease(const QMouseEvent& event);
    bool mouseDoubleClick(const QMouseEvent& event);
    bool wheel(const QWheelEvent& event);
    bool touch(const QTouchEvent& event);
    bool nativeGesture(const QNativeGestureEvent& event);
    /// Whether to open the context menu now. When the right button is bound to a drag or click,
    /// its release decides instead: a click that didn't move opens the menu (if the click does
    /// nothing else), and menus asked for meanwhile are dropped.
    [[nodiscard]] bool contextMenu(const QContextMenuEvent& event);
    void               leave();

    /// The action of the drag under way: PAN, BOX_ZOOM, or NONE.
    [[nodiscard]] PlotAction dragAction() const noexcept;
    /// Whether the user is moving the legend.
    [[nodiscard]] bool isDraggingLegend() const noexcept;
    /// The box being dragged out to zoom, in widget coordinates (nothing while it is too small).
    [[nodiscard]] std::optional<QRectF> zoomBox() const;

private:
    // An axis being panned or zoomed, with the value that must stay under the pointer.
    struct Target
    {
        Axis*             axis = nullptr;
        core::AxisMapping mapping;  // when the gesture began
        double            value      = 0.0;
        bool              horizontal = true;
    };
    // Which axes a zoom box zooms.
    enum class BoxAxes : std::uint8_t
    {
        NONE,
        X,
        Y,
        BOTH,
    };
    // Gestures without a press and release: a step lasts while their events keep coming.
    enum class Continuous : std::uint8_t
    {
        NONE,
        WHEEL,
        SCROLL,
        PINCH,
    };

    [[nodiscard]] const InputBindings& bindings() const;
    [[nodiscard]] std::vector<Target> targetsFor(PlotAction action, Region region, QPointF position,
                                                 const PlotLayout& layout) const;
    [[nodiscard]] const AxisLayout&   layoutOf(const PlotLayout& layout, const Axis* axis) const;
    // Pans each target's axis so its value lies under @p pointer in the current layout.
    void keepUnderPointer(const std::vector<Target>& targets, QPointF pointer) const;
    // Zooms the targets by @p factor (< 1 zooms in) about @p position.
    void zoom(const std::vector<Target>& targets, QPointF position, double factor) const;
    // Performs a CLICK or DOUBLE_CLICK action; returns whether there was one.
    bool perform(PlotAction action);
    // Starts or continues a history step of continuous gesture events.
    void continueStep(Continuous kind);

    bool beginDrag(QPointF position, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void dragTo(QPointF position);
    // Ends the drag at @p position; without @p click, a drag that didn't move is no click.
    void                  endDrag(QPointF position, bool click);
    [[nodiscard]] BoxAxes boxAxes() const;
    void                  applyBoxZoom();
    void beginPinch(QPointF first, QPointF second, Qt::KeyboardModifiers modifiers);
    void pinchTo(QPointF first, QPointF second);
    void endPinch();
    // Touch events, given the points still down. A touch that ends (@p lifted, not cancelled)
    // without moving is a tap; two close together are a double tap.
    bool touchBegin(const std::vector<QPointF>& down, Qt::KeyboardModifiers modifiers);
    void touchUpdate(const std::vector<QPointF>& down, Qt::KeyboardModifiers modifiers);
    void touchEnd(QPointF position, bool lifted);
    void tapped(QPointF position);

    PlotWidget*                 m_plot;
    std::function<PlotLayout()> m_layout;

    struct Drag
    {
        bool                  active = false;
        PlotAction            action = PlotAction::NONE;  // NONE: a press that is only a click
        bool                  legend = false;  // on the legend: moves it or clicks an entry
        Qt::MouseButton       button = Qt::NoButton;
        Qt::KeyboardModifiers modifiers;
        Region                region = Region::PLOT;
        QPointF               start;
        QPointF               current;
        QRectF                plot;           // the plot area when it began
        bool                  moved = false;  // beyond the drag distance: not a click
        std::vector<Target>   targets;
    };
    Drag m_drag;
    // A drag on the legend: the entry pressed, and where the legend was.
    struct LegendGrab
    {
        QPointer<Series> entry;
        QRectF           box;
    };
    LegendGrab m_legendGrab;
    // A right-button drag or click just ended: a context menu asked for now (on the release) is
    // dropped.
    bool m_dropMenu = false;

    struct Pinch
    {
        bool                active = false;
        QPointF             first;  // the two touch points when it began
        QPointF             second;
        std::vector<Target> targets;
    };
    Pinch m_pinch;

    struct Touch
    {
        bool          moved = false;  // the touch went beyond the drag distance
        QElapsedTimer pressed;        // since the first finger went down
        QElapsedTimer lastTap;
        QPointF       lastTapPosition;
    };
    Touch m_touch;

    Continuous    m_continuous = Continuous::NONE;
    QElapsedTimer m_continuousTimer;
};

}  // namespace rocketplot
