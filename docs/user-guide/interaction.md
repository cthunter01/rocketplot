# Interaction

[User Guide](index.md) · previous: [Annotations](annotations.md) · next: [The legend](legend.md)

A plot can be explored with the mouse, a trackpad or a touchscreen without any code. This chapter lists what the
user can do, and how your code can follow along, add to it, or change it.

## What the user can do

| Gesture | What it does |
| --- | --- |
| Drag with the left button | Pan: the view moves with the pointer |
| Shift + drag | Zoom to the box dragged out |
| Mouse wheel | Zoom in and out about the pointer |
| Ctrl + wheel, Shift + wheel | Zoom the x axis only, the y axes only |
| Double-click | Reset the view: every axis back to autoscale |
| Back and forward buttons of the mouse | Step through the view history |
| Two-finger scroll on a trackpad | Pan |
| Ctrl + two-finger scroll | Zoom |
| Pinch (trackpad or touchscreen) | Zoom |
| One-finger drag on a touchscreen | Pan |
| Right-click | The context menu |

Three details make these do what people expect:

- **Over an axis, a gesture applies to that axis alone.** Drag on the x axis to pan in time only; turn the wheel
  over the y axis to zoom y only.
- **A thin zoom box zooms one axis.** Drag a box that is wide but only a few pixels high and the x axis zooms to
  it while y stays as it is; the same for a tall, narrow box and y.
- **Zooming keeps the point under the pointer where it is.**

Panning or zooming an axis turns its autoscale off ([Axes](axes.md#range-and-autoscale)). The double-click turns
it back on.

The legend has gestures of its own; see [The legend](legend.md).

## The view history

Every pan, zoom and reset by the user is a step in the plot's view history, like pages in a browser.

<!-- example: interaction-history -->
```cpp
plot->back();     // the view before the user's last pan, zoom or reset
plot->forward();  // and forward again

// A toolbar button that is only enabled when there is something to go back to.
QObject::connect(plot, &rocketplot::PlotWidget::historyChanged, backButton,
                 [plot, backButton] { backButton->setEnabled(plot->canGoBack()); });
QObject::connect(backButton, &QPushButton::clicked, plot, &rocketplot::PlotWidget::back);
```

- A *view* is the range and the autoscale setting of every axis.
- A burst of wheel steps (less than half a second apart) counts as one step, so one `back()` undoes one zoom,
  however many notches it took.
- The history holds the last 50 views.
- Ranges your code sets (`setRange()`, `restoreState()`) are not recorded. `resetView()` is.
- Linked plots share one history; see [Several plots](multiple-plots.md).

## The crosshair

The crosshair is a pair of lines through the pointer, with the coordinates written on the axes. With it on, each
legend entry also shows its series' value at the pointer's x.

![A plot with the crosshair on: coordinates on the axes, values in the legend](images/interaction-crosshair.png)

<!-- example: interaction-crosshair -->
```cpp
plot->setCrosshairEnabled(true);  // off by default; the context menu has a switch for it
```

The value in the legend is the y of the series' point nearest to the crosshair's x. It is shown for series that
are sorted by x, and only while the crosshair is within the series' x range.

To show the position elsewhere, such as in a status bar:

<!-- example: interaction-crosshair-signal -->
```cpp
QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, status, [plot, status] {
    if (const std::optional<QPointF> at = plot->crosshairPosition())
    {
        status->setText(QString("t = %1 s").arg(at->x(), 0, 'f', 1));
    }
    else
    {
        status->clear();  // the pointer left the plot
    }
});
```

The y of `crosshairPosition()` is on the left y axis. A plot that shows the crosshair of a linked plot
([Several plots](multiple-plots.md)) has an x there but no y of its own: y is NaN.

### A crosshair that follows the data

To begin with the crosshair is wherever the pointer is. It can go to the data instead, so that what it reads out
is a point of a series and not a place beside it:

<!-- example: interaction-crosshair-mode -->
```cpp
plot->setCrosshairEnabled(true);
plot->setCrosshairMode(rocketplot::CrosshairMode::TRACE);  // FREE to begin with
```

| Mode | Where the crosshair is |
| --- | --- |
| `FREE` | At the pointer (the default) |
| `SNAP` | On the data point nearest the pointer, while one is within 20 pixels of it. Elsewhere, at the pointer |
| `TRACE` | On the data, wherever the pointer is: at the pointer's x, on the series nearest the pointer there |

![A tracing crosshair: on a point of the nearer series, with the point's values on the axes](images/interaction-crosshair-trace.png)

The pointer is in the same place here as in the figure above, well above both lines. The crosshair has gone to
the nearer line, at the pointer's x.

On a data point the lines cross on the point, under a marker in the color of its series, and the tags on the
axes give the point's own x and y. A few things to know:

- **It is always a point of the data**, never a place between two points. Where a line has few points, the
  crosshair steps from one to the next. Nothing is interpolated.
- **Every visible series counts**, lines and scatter series alike. A hidden series, a gap (NaN) and a point
  outside the plot area can't be landed on.
- **`SNAP` measures to the points, not to the line.** On a line of few points, halfway between two of them is
  away from the data. `TRACE` is the mode that stays on such a line.
- **`TRACE` asks each series for its point at the pointer's x**: the nearest of those in the pointer's pixel
  column, or, where the points are farther apart, the nearer of the two on either side. Of those, the series
  whose point is nearest the pointer wins. A series has none before its first point and after its last. The
  crosshair is at the pointer only where no series has a point to show.
- **A series that isn't sorted by x** (a scatter cloud, a curve that turns back) has no single point at an x.
  `TRACE` takes its point nearest the pointer, however far away.
- **With two y axes**, the y tag is on the axis the point's series is drawn against, and the other axis has none.
- **In the legend**, the entry of the series the crosshair is on shows that point's value, also for a series
  that isn't sorted by x.
- **Linked plots** show their line at the x of the point.

To find out which point it is:

<!-- example: interaction-crosshair-point -->
```cpp
QObject::connect(plot, &rocketplot::PlotWidget::crosshairMoved, status, [plot, status] {
    if (const rocketplot::Series* series = plot->crosshairSeries())
    {
        // On a data point: of this series, and this one of its points.
        const std::size_t index = plot->crosshairIndex().value_or(0);
        status->setText(QString("%1: %2 km at t = %3 s")
                            .arg(series->name())
                            .arg(series->y(index), 0, 'f', 2)
                            .arg(series->x(index), 0, 'f', 1));
    }
});
```

`crosshairSeries()` is null while the crosshair is at the pointer, or not shown. `crosshairPosition()` gives the
point too: its x, and its y if the series is on the left y axis. For a point on the right y axis, read the y from
the series as above: the y of `crosshairPosition()` is what the left axis reads at that height.

`crosshairMoved()` is also emitted when the data changes under a crosshair that follows it, as when points
arrive in a live plot.

## Following the view

<!-- example: interaction-view-signal -->
```cpp
QObject::connect(plot, &rocketplot::PlotWidget::viewChanged, status, [plot, status] {
    // The range of an axis changed: by the user, by autoscale or by code.
    const rocketplot::Range shown = plot->xAxis()->range();
    status->setText(QString("Showing %1 s").arg(shown.span(), 0, 'f', 1));
});
```

`viewChanged()` is emitted each time the range of an axis changes, whatever caused it; a pan that moves x and y
emits it twice. Don't change the same plot's ranges from this signal without a guard, or the change calls you
again.

## The context menu

A right-click opens a menu:

- **Back**, **Forward**, **Reset view**
- **Crosshair**, a switch, and what the crosshair follows: **Free**, **Snap to data** or **Trace data**.
  Choosing one of these turns the crosshair on
- **Copy image** and **Export…** ([Export](export.md))

To add entries of your own, connect to `contextMenuAboutToShow`. It hands you the menu, already filled, and where
it was asked for.

<!-- example: interaction-menu -->
```cpp
QObject::connect(plot, &rocketplot::PlotWidget::contextMenuAboutToShow, plot,
                 [plot](QMenu* menu, QPointF position) {
                     // Where the menu was asked for, in data coordinates.
                     const double x = plot->mapToData(position).x();
                     menu->addSeparator();
                     menu->addAction(QString("Mark t = %1 s").arg(x, 0, 'f', 1), plot,
                                     [plot, x] { plot->addEvent(x, "Mark"); });
                 });
```

The menu is deleted when it closes, so add to it each time.

For no menu, set the widget's context menu policy to `Qt::NoContextMenu`; to show a menu that is entirely yours,
set it to `Qt::CustomContextMenu` and handle `QWidget::customContextMenuRequested`, as for any widget.

## Changing what the gestures do

`InputBindings` maps gestures to actions. A binding is a gesture, a mouse button and modifier keys on one side,
and an action on the other.

<!-- example: interaction-bindings -->
```cpp
rocketplot::InputBindings bindings = rocketplot::InputBindings::defaults();
// Zoom to a box with the right button too, and let the wheel zoom the time axis only.
bindings.bind(rocketplot::Gesture::DRAG, Qt::RightButton, Qt::NoModifier,
              rocketplot::PlotAction::BOX_ZOOM);
bindings.bind(rocketplot::Gesture::WHEEL, Qt::NoModifier, rocketplot::PlotAction::ZOOM_X);
// A double click does nothing.
bindings.bind(rocketplot::Gesture::DOUBLE_CLICK, Qt::LeftButton, Qt::NoModifier,
              rocketplot::PlotAction::NONE);
plot->setInputBindings(bindings);
```

| Gesture | Is |
| --- | --- |
| `DRAG` | Press a button and move. A one-finger drag on a touchscreen is the left button |
| `CLICK` | Press and release without moving |
| `DOUBLE_CLICK` | Also a double tap |
| `WHEEL` | A mouse wheel |
| `SCROLL` | Two-finger scrolling on a trackpad |
| `PINCH` | Two fingers moving apart or together |

| Action | Does | For gestures |
| --- | --- | --- |
| `PAN` | Move the view with the pointer | `DRAG`, `SCROLL` |
| `ZOOM`, `ZOOM_X`, `ZOOM_Y` | Zoom about the pointer: all axes, x only, y only | `WHEEL`, `SCROLL`, `PINCH` |
| `BOX_ZOOM` | Zoom to the box dragged out | `DRAG` |
| `RESET` | Back to autoscale | `CLICK`, `DOUBLE_CLICK` |
| `BACK`, `FORWARD` | Step through the view history | `CLICK`, `DOUBLE_CLICK` |
| `NONE` | Nothing: removes the binding | any |

The modifiers must match exactly: a binding for Shift + drag does not fire for Ctrl + Shift + drag. `WHEEL`,
`SCROLL` and `PINCH` have no button; use the overload of `bind()` without one.

If you bind a drag with the right button, the context menu waits for the release and opens only if the pointer
didn't move. If you bind a right *click*, the click does its action instead of opening the menu.

Bindings belong to one plot. To use the same ones everywhere, keep an `InputBindings` object and pass it to each
plot's `setInputBindings()`.

## A plot that is only looked at

For a plot on a dashboard that nobody should be able to shift:

<!-- example: interaction-static -->
```cpp
plot->setInputBindings(rocketplot::InputBindings());  // no gesture does anything
plot->setContextMenuPolicy(Qt::NoContextMenu);        // no menu
plot->legend()->setInteractive(false);                // the legend doesn't react either
```

Next: [The legend](legend.md).
