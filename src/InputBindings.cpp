#include "rocketplot/InputBindings.h"

#include <QList>
#include <Qt>

#include "rocketplot/enums.h"

namespace rocketplot
{

namespace
{

// The modifiers bindings tell apart; others (the keypad flag) are ignored.
constexpr Qt::KeyboardModifiers kBindingModifiers =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

}  // namespace

InputBindings InputBindings::defaults()
{
    InputBindings bindings;
    bindings.bind(Gesture::DRAG, Qt::LeftButton, Qt::NoModifier, PlotAction::PAN);
    bindings.bind(Gesture::DRAG, Qt::LeftButton, Qt::ShiftModifier, PlotAction::BOX_ZOOM);
    bindings.bind(Gesture::DOUBLE_CLICK, Qt::LeftButton, Qt::NoModifier, PlotAction::RESET);
    bindings.bind(Gesture::CLICK, Qt::BackButton, Qt::NoModifier, PlotAction::BACK);
    bindings.bind(Gesture::CLICK, Qt::ForwardButton, Qt::NoModifier, PlotAction::FORWARD);
    bindings.bind(Gesture::WHEEL, Qt::NoModifier, PlotAction::ZOOM);
    bindings.bind(Gesture::WHEEL, Qt::ControlModifier, PlotAction::ZOOM_X);
    bindings.bind(Gesture::WHEEL, Qt::ShiftModifier, PlotAction::ZOOM_Y);
    bindings.bind(Gesture::SCROLL, Qt::NoModifier, PlotAction::PAN);
    bindings.bind(Gesture::SCROLL, Qt::ControlModifier, PlotAction::ZOOM);
    bindings.bind(Gesture::PINCH, Qt::NoModifier, PlotAction::ZOOM);
    return bindings;
}

void InputBindings::bind(Gesture gesture, Qt::MouseButton button, Qt::KeyboardModifiers modifiers,
                         PlotAction action)
{
    modifiers &= kBindingModifiers;
    m_bindings.removeIf([&](const Binding& binding) {
        return binding.gesture == gesture && binding.button == button &&
               binding.modifiers == modifiers;
    });
    if (action != PlotAction::NONE)
    {
        m_bindings.append({
            .gesture   = gesture,
            .button    = button,
            .modifiers = modifiers,
            .action    = action,
        });
    }
}

void InputBindings::bind(Gesture gesture, Qt::KeyboardModifiers modifiers, PlotAction action)
{
    bind(gesture, Qt::NoButton, modifiers, action);
}

void InputBindings::clear()
{
    m_bindings.clear();
}

PlotAction InputBindings::action(Gesture gesture, Qt::MouseButton button,
                                 Qt::KeyboardModifiers modifiers) const
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
    return PlotAction::NONE;
}

}  // namespace rocketplot
