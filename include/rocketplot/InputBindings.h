#pragma once

#include <QList>
#include <Qt>

#include "rocketplot/enums.h"
#include "rocketplot/export.h"

namespace rocketplot
{

/// Which pointer gestures do what in a PlotWidget (PlotWidget::setInputBindings()). A binding maps
/// a gesture, with its mouse button and the modifier keys held, to an action; the modifiers must
/// match exactly. Gestures without a button (WHEEL, SCROLL, PINCH) use Qt::NoButton.
///
/// The defaults():
/// | Gesture                     | Action           |
/// | --------------------------- | ---------------- |
/// | Left-drag                   | PAN              |
/// | Shift + left-drag           | BOX_ZOOM         |
/// | Double-click (left)         | RESET            |
/// | Back / forward mouse button | BACK / FORWARD   |
/// | Wheel                       | ZOOM             |
/// | Ctrl + wheel, Shift + wheel | ZOOM_X, ZOOM_Y   |
/// | Trackpad scroll             | PAN              |
/// | Ctrl + trackpad scroll      | ZOOM             |
/// | Pinch                       | ZOOM             |
///
/// @code
/// rocketplot::InputBindings bindings = rocketplot::InputBindings::defaults();
/// // Zoom with the right button too, and let the wheel only zoom x.
/// bindings.bind(rocketplot::Gesture::DRAG, Qt::RightButton, Qt::NoModifier,
///               rocketplot::PlotAction::BOX_ZOOM);
/// bindings.bind(rocketplot::Gesture::WHEEL, Qt::NoModifier, rocketplot::PlotAction::ZOOM_X);
/// plot->setInputBindings(bindings);
/// @endcode
///
/// A right click opens the plot's context menu. With a right-drag bound, the menu waits for the
/// release and opens only if the pointer didn't move; with a right click bound, the click does its
/// action instead.
class ROCKETPLOT_EXPORT InputBindings
{
public:
    struct Binding
    {
        Gesture               gesture   = Gesture::DRAG;
        Qt::MouseButton       button    = Qt::NoButton;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        PlotAction            action    = PlotAction::NONE;

        friend bool operator==(const Binding&, const Binding&) = default;
    };

    /// No bindings: every gesture does nothing.
    InputBindings() = default;
    /// The bindings a PlotWidget starts with (see the table above).
    [[nodiscard]] static InputBindings defaults();

    /// Makes @p gesture with @p button and @p modifiers do @p action, replacing what it did.
    /// PlotAction::NONE removes the binding.
    void bind(Gesture gesture, Qt::MouseButton button, Qt::KeyboardModifiers modifiers,
              PlotAction action);
    /// The same for a gesture without a button (WHEEL, SCROLL, PINCH).
    void bind(Gesture gesture, Qt::KeyboardModifiers modifiers, PlotAction action);
    /// Removes every binding.
    void clear();

    /// What @p gesture with @p button and @p modifiers does (the keypad modifier is ignored).
    [[nodiscard]] PlotAction     action(Gesture gesture, Qt::MouseButton button,
                                        Qt::KeyboardModifiers modifiers) const;
    [[nodiscard]] QList<Binding> bindings() const { return m_bindings; }

    friend bool operator==(const InputBindings&, const InputBindings&) = default;

private:
    QList<Binding> m_bindings;
};

}  // namespace rocketplot
