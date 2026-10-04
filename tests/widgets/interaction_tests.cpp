// Pointer, trackpad and touch interaction with a PlotWidget: box zoom, view history, input
// bindings, the crosshair and the context menu.

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QNativeGestureEvent>
#include <QPoint>
#include <QPointF>
#include <QPointingDevice>
#include <QRectF>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>
#include <Qt>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "rocketplot/Axis.h"
#include "rocketplot/InputBindings.h"
#include "rocketplot/PlotWidget.h"
#include "rocketplot/Range.h"
#include "rocketplot/Theme.h"
#include "rocketplot/enums.h"

namespace
{

using rocketplot::Gesture;
using rocketplot::InputBindings;
using rocketplot::PlotAction;
using rocketplot::PlotWidget;
using rocketplot::Range;

constexpr double kTolerance = 1e-9;

QPoint pixel(QPointF point)
{
    return point.toPoint();
}

// The open popup menu, if any.
QMenu* openMenu()
{
    return qobject_cast<QMenu*>(QApplication::activePopupWidget());
}

QStringList actionTexts(const QMenu& menu)
{
    QStringList texts;
    for (const QAction* action : menu.actions())
    {
        if (!action->isSeparator())
        {
            texts.append(action->text());
        }
    }
    return texts;
}

class InteractionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_plot.resize(800, 600);
        m_plot.setThemeMode(rocketplot::ThemeMode::LIGHT);
        m_plot.addLine(std::vector<double>{0, 5, 10}, std::vector<double>{-5, 5, 0}, "first");
        m_plot.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&m_plot));
    }

    void TearDown() override
    {
        // A menu left open would take the next test's input.
        while (QWidget* popup = QApplication::activePopupWidget())
        {
            popup->close();
        }
    }

    [[nodiscard]] QPointF center() const { return m_plot.plotArea().center(); }
    [[nodiscard]] Range   x() const { return m_plot.xAxis()->range(); }
    [[nodiscard]] Range   y() const { return m_plot.yAxis()->range(); }

    void wheel(QPointF position, int units, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QWheelEvent event(position, m_plot.mapToGlobal(position), QPoint(), QPoint(0, units),
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&m_plot, &event);
    }

    // Two-finger scrolling on a trackpad (it comes with a scroll phase).
    void scroll(QPointF position, QPoint pixels, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QWheelEvent event(position, m_plot.mapToGlobal(position), pixels, pixels * 2, Qt::NoButton,
                          modifiers, Qt::ScrollUpdate, false);
        QApplication::sendEvent(&m_plot, &event);
    }

    void pinch(QPointF position, double value)
    {
        QNativeGestureEvent event(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(),
                                  2, position, position, m_plot.mapToGlobal(position), value,
                                  QPointF());
        QApplication::sendEvent(&m_plot, &event);
    }

    void drag(QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton,
              Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QTest::mousePress(&m_plot, button, modifiers, from);
        QTest::mouseMove(&m_plot, (from + to) / 2);
        QTest::mouseMove(&m_plot, to);
        QTest::mouseRelease(&m_plot, button, modifiers, to);
    }

    PlotWidget m_plot;
};

// Box zoom
// --------------------------------------------------------------------------------------------------------

TEST_F(InteractionTest, ShiftDragZoomsToTheBox)
{
    const QRectF  area = m_plot.plotArea();
    const QPoint  from = pixel(area.topLeft() + QPointF(100, 80));
    const QPoint  to   = pixel(area.topLeft() + QPointF(300, 240));
    const QPointF a    = m_plot.mapToData(from);
    const QPointF b    = m_plot.mapToData(to);
    drag(from, to, Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_NEAR(x().min, a.x(), kTolerance);
    EXPECT_NEAR(x().max, b.x(), kTolerance);
    EXPECT_NEAR(y().min, b.y(), kTolerance);
    EXPECT_NEAR(y().max, a.y(), kTolerance);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_FALSE(m_plot.yAxis()->autoscale());
}

TEST_F(InteractionTest, AThinBoxZoomsOneAxis)
{
    const Range   before = y();
    const QPoint  from   = pixel(center() - QPointF(150, 0));
    const QPoint  to     = pixel(center() + QPointF(150, 6));
    const QPointF a      = m_plot.mapToData(from);
    const QPointF b      = m_plot.mapToData(to);
    drag(from, to, Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_NEAR(x().min, a.x(), kTolerance);
    EXPECT_NEAR(x().max, b.x(), kTolerance);
    EXPECT_EQ(y(), before);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());

    const Range xBefore = x();
    drag(pixel(center() - QPointF(0, 100)), pixel(center() + QPointF(4, 100)), Qt::LeftButton,
         Qt::ShiftModifier);
    EXPECT_EQ(x(), xBefore);
    EXPECT_LT(y().span(), before.span());
}

TEST_F(InteractionTest, ABoxOverAnAxisZoomsThatAxis)
{
    const QRectF area   = m_plot.plotArea();
    const Range  before = y();
    drag(pixel(QPointF(area.left() + 50, area.bottom() + 8)),
         pixel(QPointF(area.left() + 250, area.bottom() + 30)), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_EQ(y(), before);
}

TEST_F(InteractionTest, ATinyBoxDoesNothing)
{
    const Range before = x();
    drag(pixel(center()), pixel(center() + QPointF(3, 3)), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_EQ(x(), before);
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    EXPECT_FALSE(m_plot.canGoBack());
}

TEST_F(InteractionTest, TheBoxIsDrawnWhileDragging)
{
    const QPoint from = pixel(center() - QPointF(100, 80));
    const QPoint to   = pixel(center() + QPointF(100, 80));
    QTest::mousePress(&m_plot, Qt::LeftButton, Qt::ShiftModifier, from);
    QTest::mouseMove(&m_plot, to);
    const QImage image = m_plot.grab().toImage();
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::ShiftModifier, to);
    EXPECT_EQ(image.pixelColor(from.x(), center().toPoint().y()), m_plot.theme().zoomBoxBorder);
    EXPECT_NE(m_plot.grab().toImage().pixelColor(from.x(), center().toPoint().y()),
              m_plot.theme().zoomBoxBorder);
}

// History
// --------------------------------------------------------------------------------------------------------

TEST_F(InteractionTest, BackAndForwardStepThroughViews)
{
    const Range      fitted = x();
    const QSignalSpy history(&m_plot, &PlotWidget::historyChanged);
    EXPECT_FALSE(m_plot.canGoBack());
    wheel(center(), 120);
    wheel(center(), 120);  // the same burst: one step
    const Range zoomed = x();
    EXPECT_TRUE(m_plot.canGoBack());
    EXPECT_FALSE(m_plot.canGoForward());
    EXPECT_GE(history.count(), 1);

    m_plot.back();
    EXPECT_EQ(x(), fitted);
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    EXPECT_FALSE(m_plot.canGoBack());
    EXPECT_TRUE(m_plot.canGoForward());

    m_plot.forward();
    EXPECT_EQ(x(), zoomed);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_FALSE(m_plot.canGoForward());
}

TEST_F(InteractionTest, ANewStepDropsTheViewsAhead)
{
    wheel(center(), 120);
    m_plot.back();
    ASSERT_TRUE(m_plot.canGoForward());
    drag(pixel(center()), pixel(center() + QPointF(50, 20)));
    EXPECT_FALSE(m_plot.canGoForward());
    EXPECT_TRUE(m_plot.canGoBack());
}

TEST_F(InteractionTest, PanAndBoxZoomAreSteps)
{
    const Range fitted = x();
    drag(pixel(center()), pixel(center() + QPointF(80, 0)));
    const Range panned = x();
    drag(pixel(center() - QPointF(100, 80)), pixel(center() + QPointF(100, 80)), Qt::LeftButton,
         Qt::ShiftModifier);
    m_plot.back();
    EXPECT_EQ(x(), panned);
    m_plot.back();
    EXPECT_EQ(x(), fitted);
}

TEST_F(InteractionTest, AClickIsNoStep)
{
    QTest::mouseClick(&m_plot, Qt::LeftButton, Qt::NoModifier, pixel(center()));
    EXPECT_FALSE(m_plot.canGoBack());
}

TEST_F(InteractionTest, ResetIsAStep)
{
    wheel(center(), 240);
    const Range zoomed = x();
    QTest::mouseDClick(&m_plot, Qt::LeftButton, Qt::NoModifier, pixel(center()));
    // As a real double click ends, and so that QTest doesn't go on thinking the button is down.
    QTest::mouseRelease(&m_plot, Qt::LeftButton, Qt::NoModifier, pixel(center()));
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
    m_plot.back();
    EXPECT_EQ(x(), zoomed);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
}

TEST_F(InteractionTest, MouseBackAndForwardButtons)
{
    const Range fitted = x();
    wheel(center(), 120);
    const Range zoomed = x();
    QTest::mouseClick(&m_plot, Qt::BackButton, Qt::NoModifier, pixel(center()));
    EXPECT_EQ(x(), fitted);
    QTest::mouseClick(&m_plot, Qt::ForwardButton, Qt::NoModifier, pixel(center()));
    EXPECT_EQ(x(), zoomed);
}

// Bindings and gestures
// ---------------------------------------------------------------------------------------------

TEST_F(InteractionTest, BindingsCanBeChanged)
{
    InputBindings bindings;  // nothing
    bindings.bind(Gesture::WHEEL, Qt::NoModifier, PlotAction::ZOOM_X);
    bindings.bind(Gesture::DRAG, Qt::RightButton, Qt::NoModifier, PlotAction::BOX_ZOOM);
    m_plot.setInputBindings(bindings);
    EXPECT_EQ(m_plot.inputBindings(), bindings);

    const Range yBefore = y();
    wheel(center(), 120);
    EXPECT_FALSE(m_plot.xAxis()->autoscale());
    EXPECT_EQ(y(), yBefore);

    const Range xBefore = x();
    drag(pixel(center()), pixel(center() + QPointF(80, 40)));  // left: nothing now
    EXPECT_EQ(x(), xBefore);
    EXPECT_EQ(y(), yBefore);

    drag(pixel(center() - QPointF(100, 80)), pixel(center() + QPointF(100, 80)), Qt::RightButton);
    EXPECT_LT(x().span(), xBefore.span());
    EXPECT_FALSE(m_plot.yAxis()->autoscale());
    EXPECT_EQ(openMenu(), nullptr);  // a right-drag opens no menu
}

TEST_F(InteractionTest, TrackpadScrollPans)
{
    const Range  before = x();
    const double width  = m_plot.plotArea().width();
    scroll(center(), QPoint(30, 0));
    EXPECT_NEAR(x().min, before.min - (30.0 * before.span() / width), kTolerance);
    EXPECT_NEAR(x().span(), before.span(), kTolerance);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());  // a sideways swipe leaves y alone
    EXPECT_TRUE(m_plot.canGoBack());
}

TEST_F(InteractionTest, CtrlTrackpadScrollZooms)
{
    const Range before = x();
    scroll(center(), QPoint(0, 60), Qt::ControlModifier);
    EXPECT_LT(x().span(), before.span());
}

TEST_F(InteractionTest, PinchZoomsAboutThePointer)
{
    const QPointF position = center() + QPointF(-100, 50);
    const QPointF before   = m_plot.mapToData(position);
    const Range   span     = x();
    pinch(position, 0.5);
    EXPECT_NEAR(x().span(), span.span() / 1.5, kTolerance);
    const QPointF after = m_plot.mapToData(position);
    EXPECT_NEAR(after.x(), before.x(), kTolerance);
    EXPECT_NEAR(after.y(), before.y(), kTolerance);
}

class TouchTest : public InteractionTest
{
protected:
    // Ours to delete; deleting it unregisters it.
    std::unique_ptr<QPointingDevice> m_device{QTest::createTouchDevice()};
    QPointingDevice*                 m_screen = m_device.get();
};

TEST_F(TouchTest, OneFingerPans)
{
    const Range  before = x();
    const QPoint start  = pixel(center());
    QTest::touchEvent(&m_plot, m_screen).press(0, start);
    QTest::touchEvent(&m_plot, m_screen).move(0, start + QPoint(50, 0));
    QTest::touchEvent(&m_plot, m_screen).move(0, start + QPoint(100, 0));
    QTest::touchEvent(&m_plot, m_screen).release(0, start + QPoint(100, 0));
    EXPECT_NEAR(x().min, before.min - (100.0 * before.span() / m_plot.plotArea().width()),
                kTolerance);
    EXPECT_TRUE(m_plot.canGoBack());
}

TEST_F(TouchTest, PinchZoomsAlongTheSpread)
{
    const Range  xBefore = x();
    const Range  yBefore = y();
    const QPoint middle  = pixel(center());
    QTest::touchEvent(&m_plot, m_screen)
        .press(0, middle - QPoint(60, 0))
        .press(1, middle + QPoint(60, 0));
    QTest::touchEvent(&m_plot, m_screen)
        .move(0, middle - QPoint(90, 0))
        .move(1, middle + QPoint(90, 0));
    QTest::touchEvent(&m_plot, m_screen)
        .move(0, middle - QPoint(120, 0))
        .move(1, middle + QPoint(120, 0));
    QTest::touchEvent(&m_plot, m_screen)
        .release(0, middle - QPoint(120, 0))
        .release(1, middle + QPoint(120, 0));
    // Spread twice as far apart sideways: x zoomed in twice; y, with no vertical spread, alone.
    EXPECT_NEAR(x().span(), xBefore.span() / 2.0, 1e-6);
    EXPECT_EQ(y(), yBefore);
    EXPECT_TRUE(m_plot.yAxis()->autoscale());
}

TEST_F(TouchTest, DoubleTapResets)
{
    wheel(center(), 240);
    ASSERT_FALSE(m_plot.xAxis()->autoscale());
    const QPoint point = pixel(center());
    for (int tap = 0; tap < 2; ++tap)
    {
        QTest::touchEvent(&m_plot, m_screen).press(0, point);
        QTest::touchEvent(&m_plot, m_screen).release(0, point);
    }
    EXPECT_TRUE(m_plot.xAxis()->autoscale());
}

// Crosshair
// --------------------------------------------------------------------------------------------------------

TEST_F(InteractionTest, CrosshairFollowsThePointer)
{
    const QPoint position = pixel(center() + QPointF(-50, 30));
    const QImage plain    = m_plot.grab().toImage();
    EXPECT_FALSE(m_plot.crosshairPosition());
    m_plot.setCrosshairEnabled(true);
    const QSignalSpy moved(&m_plot, &PlotWidget::crosshairMoved);
    QTest::mouseMove(&m_plot, position);
    ASSERT_TRUE(m_plot.crosshairPosition());
    const QPointF crosshair = m_plot.crosshairPosition().value_or(QPointF());
    EXPECT_NEAR(crosshair.x(), m_plot.mapToData(position).x(), kTolerance);
    EXPECT_NEAR(crosshair.y(), m_plot.mapToData(position).y(), kTolerance);
    EXPECT_GE(moved.count(), 1);

    // A line down through the pointer.
    const QPoint above(position.x(), static_cast<int>(m_plot.plotArea().top()) + 3);
    EXPECT_NE(m_plot.grab().toImage().pixelColor(above), plain.pixelColor(above));

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&m_plot, &leave);
    EXPECT_FALSE(m_plot.crosshairPosition());
    QTest::mouseMove(&m_plot, position);
    m_plot.setCrosshairEnabled(false);
    EXPECT_FALSE(m_plot.crosshairPosition());
}

TEST_F(InteractionTest, CrosshairOnlyOverThePlotArea)
{
    m_plot.setCrosshairEnabled(true);
    const QRectF area = m_plot.plotArea();
    QTest::mouseMove(&m_plot, pixel(QPointF(area.center().x(), area.bottom() + 10)));
    EXPECT_FALSE(m_plot.crosshairPosition());
}

// Context menu
// ---------------------------------------------------------------------------------------------------

TEST_F(InteractionTest, ContextMenuHasTheViewActions)
{
    const QSignalSpy  aboutToShow(&m_plot, &PlotWidget::contextMenuAboutToShow);
    QContextMenuEvent event(QContextMenuEvent::Mouse, pixel(center()),
                            m_plot.mapToGlobal(pixel(center())));
    QApplication::sendEvent(&m_plot, &event);
    QMenu* menu = openMenu();
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(aboutToShow.count(), 1);
    EXPECT_EQ(actionTexts(*menu),
              (QStringList{"Back", "Forward", "Reset view", "Crosshair", "Copy image", "Export…"}));
    EXPECT_FALSE(menu->actions().front()->isEnabled());  // nothing to go back to
    // By name: triggering Export… instead would wait for a file dialog that nobody answers.
    QAction* crosshair = nullptr;
    for (QAction* action : menu->actions())
    {
        crosshair = action->text() == "Crosshair" ? action : crosshair;
    }
    ASSERT_NE(crosshair, nullptr);
    crosshair->trigger();
    EXPECT_TRUE(m_plot.isCrosshairEnabled());
    menu->close();
}

TEST_F(InteractionTest, ContextMenuPolicyApplies)
{
    m_plot.setContextMenuPolicy(Qt::NoContextMenu);
    QContextMenuEvent event(QContextMenuEvent::Mouse, pixel(center()),
                            m_plot.mapToGlobal(pixel(center())));
    QApplication::sendEvent(&m_plot, &event);
    EXPECT_EQ(openMenu(), nullptr);
}

// Through the window, as the platform delivers them: Qt then asks for the context menu itself (on
// the press on Linux and macOS, on the release on Windows).

TEST_F(InteractionTest, RightClickOpensTheMenu)
{
    const QSignalSpy aboutToShow(&m_plot, &PlotWidget::contextMenuAboutToShow);
    QTest::mouseClick(m_plot.windowHandle(), Qt::RightButton, Qt::NoModifier, pixel(center()));
    EXPECT_NE(openMenu(), nullptr);
    EXPECT_EQ(aboutToShow.count(), 1);
}

TEST_F(InteractionTest, WithARightDragTheReleaseOpensTheMenu)
{
    InputBindings bindings = InputBindings::defaults();
    bindings.bind(Gesture::DRAG, Qt::RightButton, Qt::NoModifier, PlotAction::BOX_ZOOM);
    m_plot.setInputBindings(bindings);
    const QSignalSpy aboutToShow(&m_plot, &PlotWidget::contextMenuAboutToShow);
    QWindow*         window = m_plot.windowHandle();
    const QPoint     point  = pixel(center());
    QTest::mousePress(window, Qt::RightButton, Qt::NoModifier, point);
    EXPECT_EQ(openMenu(), nullptr);  // not yet: it may become a drag
    QTest::mouseRelease(window, Qt::RightButton, Qt::NoModifier, point);
    EXPECT_NE(openMenu(), nullptr);
    EXPECT_EQ(aboutToShow.count(), 1);
}

TEST_F(InteractionTest, ARightDragOpensNoMenu)
{
    InputBindings bindings = InputBindings::defaults();
    bindings.bind(Gesture::DRAG, Qt::RightButton, Qt::NoModifier, PlotAction::BOX_ZOOM);
    m_plot.setInputBindings(bindings);
    const QSignalSpy aboutToShow(&m_plot, &PlotWidget::contextMenuAboutToShow);
    const Range      before = x();
    QWindow*         window = m_plot.windowHandle();
    const QPoint     from   = pixel(center() - QPointF(100, 80));
    const QPoint     to     = pixel(center() + QPointF(100, 80));
    QTest::mousePress(window, Qt::RightButton, Qt::NoModifier, from);
    QTest::mouseMove(window, to);
    QTest::mouseRelease(window, Qt::RightButton, Qt::NoModifier, to);
    EXPECT_EQ(openMenu(), nullptr);
    EXPECT_EQ(aboutToShow.count(), 0);
    EXPECT_LT(x().span(), before.span());
}

}  // namespace
