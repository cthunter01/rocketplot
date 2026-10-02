#include "rocketplot/InputBindings.h"

#include <Qt>

#include <gtest/gtest.h>

#include "rocketplot/enums.h"

namespace
{

using rocketplot::Gesture;
using rocketplot::InputBindings;
using rocketplot::PlotAction;

TEST(InputBindings, Defaults)
{
    const InputBindings bindings = InputBindings::defaults();
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::LeftButton, Qt::NoModifier), PlotAction::PAN);
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::LeftButton, Qt::ShiftModifier),
              PlotAction::BOX_ZOOM);
    EXPECT_EQ(bindings.action(Gesture::DOUBLE_CLICK, Qt::LeftButton, Qt::NoModifier),
              PlotAction::RESET);
    EXPECT_EQ(bindings.action(Gesture::CLICK, Qt::BackButton, Qt::NoModifier), PlotAction::BACK);
    EXPECT_EQ(bindings.action(Gesture::WHEEL, Qt::NoButton, Qt::ControlModifier),
              PlotAction::ZOOM_X);
    EXPECT_EQ(bindings.action(Gesture::SCROLL, Qt::NoButton, Qt::NoModifier), PlotAction::PAN);
    EXPECT_EQ(bindings.action(Gesture::PINCH, Qt::NoButton, Qt::NoModifier), PlotAction::ZOOM);
    // Nothing on the right button: it opens the context menu.
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::RightButton, Qt::NoModifier), PlotAction::NONE);
}

TEST(InputBindings, BindReplacesAndNoneRemoves)
{
    InputBindings bindings = InputBindings::defaults();
    bindings.bind(Gesture::WHEEL, Qt::NoModifier, PlotAction::ZOOM_X);
    EXPECT_EQ(bindings.action(Gesture::WHEEL, Qt::NoButton, Qt::NoModifier), PlotAction::ZOOM_X);
    EXPECT_EQ(bindings.bindings().size(), InputBindings::defaults().bindings().size());
    bindings.bind(Gesture::DRAG, Qt::LeftButton, Qt::NoModifier, PlotAction::NONE);
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::LeftButton, Qt::NoModifier), PlotAction::NONE);
    EXPECT_EQ(bindings.bindings().size(), InputBindings::defaults().bindings().size() - 1);
    EXPECT_NE(bindings, InputBindings::defaults());
    bindings.clear();
    EXPECT_TRUE(bindings.bindings().isEmpty());
    EXPECT_EQ(bindings, InputBindings());
}

TEST(InputBindings, ModifiersMatchExactlyExceptTheKeypad)
{
    InputBindings bindings;
    bindings.bind(Gesture::DRAG, Qt::MiddleButton, Qt::ControlModifier | Qt::KeypadModifier,
                  PlotAction::PAN);
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::MiddleButton, Qt::ControlModifier),
              PlotAction::PAN);
    EXPECT_EQ(
        bindings.action(Gesture::DRAG, Qt::MiddleButton, Qt::ControlModifier | Qt::KeypadModifier),
        PlotAction::PAN);
    EXPECT_EQ(
        bindings.action(Gesture::DRAG, Qt::MiddleButton, Qt::ControlModifier | Qt::ShiftModifier),
        PlotAction::NONE);
    EXPECT_EQ(bindings.action(Gesture::DRAG, Qt::MiddleButton, Qt::NoModifier), PlotAction::NONE);
}

}  // namespace
