# Troubleshooting

[User Guide](index.md) · previous: [Performance](performance.md)

Symptoms, their usual causes, and what to do. The debug overlay ([Performance](performance.md#the-debug-overlay))
answers many of these at a glance: it shows the ranges in view and how many points of each series are in them.

## Nothing is drawn

| Check | Why |
| --- | --- |
| Is the range where the data is? | An axis whose range was set, or that the user moved, no longer follows the data. `plot->resetView()` brings every axis back to autoscale |
| Is the series visible? | `series->isVisible()`. The user may have hidden it from the legend, and a restored state may have too |
| Are the values numbers? | Points with a NaN or infinite x or y are gaps. A series of only gaps draws nothing |
| Is an axis logarithmic? | Values that are zero or negative can't be shown on a logarithmic axis |
| Does the widget have a size? | A plot that is in no layout and was never resized or shown has none |

## The line is a scribble

The line connects the points in the order they are stored. If the x values are not in order, it runs back and
forth. Sort the data by x, or use `addScatter()` if the points have no order.

## The axis doesn't follow new data

Autoscale was turned off: by `setRange()`, by a restored state, or by the user panning or zooming. Turn it back on
with `axis->setAutoscale(true)` or `plot->resetView()`. For a live plot the user can always get back with a
double-click. See [Axes](axes.md#range-and-autoscale).

## A view doesn't show my changes

After writing to memory that a series views (`addLineView()`, `setDataView()`), call
`series->notifyDataChanged()`. See [Data](data.md#view).

## A crash after changing data that a series views

The series reads your memory; if that memory is freed or moved, the series reads garbage. The usual cause is a
`std::vector` that grew (`push_back`, `resize`) and reallocated. Give the series the new memory with
`setDataView()` after any change of size, or let the series own the data (`setData()`, `append()`).

## An exception when adding data

`std::invalid_argument`: x and y differ in size, or errors, sizes or colors are not one per point.
`std::logic_error`: `append()` on a view, or the wrong kind of `append()` for the series. See
[Data](data.md#when-the-arguments-are-wrong).

## The legend doesn't show

It shows from two entries on, and only named series have entries. For a single series, call
`plot->legend()->setVisible(true)`. See [The legend](legend.md#when-it-shows).

## The legend shows no values at the crosshair

A value is shown for series that are sorted by x, while the crosshair is within the series' x range. Check
`series->isSortedByX()`, and that `legend()->areValuesVisible()` is on. A series that isn't sorted by x shows a
value only while the crosshair is on one of its points
([a crosshair that follows the data](interaction.md#a-crosshair-that-follows-the-data)).

## The crosshair doesn't go to the data

It follows the pointer unless told otherwise: `plot->setCrosshairMode(rocketplot::CrosshairMode::SNAP)` or
`TRACE`, or the context menu's **Snap to data** and **Trace data**. With `SNAP` the pointer has to be within 20
pixels of a *point*; on a line drawn through a few points, most of the line is farther than that from any of
them, and `TRACE` is the mode to use. Neither goes to a hidden series.

## Markers don't show on a line

Markers on a line are left out while the points in view are closer together than half a marker. Zoom in and they
appear. For markers at every point always, use a scatter series.

## A time axis shows 1970, or a year far in the future

A `DATE_TIME` axis takes seconds since 1970-01-01 UTC. Values near zero (seconds since the start of a recording)
show as January 1970: use a linear axis for those, or add the start time. Values a thousand times too large are
milliseconds: divide by 1000. `rocketplot::toPlotTime()` converts a `QDateTime` or a `std::chrono` time point.

## Text shows without part of it, or with odd formatting

A string that looks like HTML is treated as Qt rich text. In such a string, write a literal `<` as `&lt;` and
`&` as `&amp;`. See [Appearance](appearance.md#rich-text).

## Colors change with the theme

Series without a color of their own take the theme's, which differ between the light and dark themes on purpose.
For a color that must stay the same, call `series->setColor()`.

## An exported image is blurry, or not the size I expected

`ExportOptions::size` is in device-independent pixels and decides the layout; `dpi` decides the number of pixels.
For print, keep `size` and raise `dpi`. See [Export](export.md#size-sharpness-and-theme).

## The mouse wheel should scroll my scroll area, not zoom the plot

Take the wheel's binding away, and the plot passes wheel events on to its parent:

<!-- example: interaction-wheel-off -->
```cpp
rocketplot::InputBindings scrolling = plot->inputBindings();
scrolling.bind(rocketplot::Gesture::WHEEL, Qt::NoModifier, rocketplot::PlotAction::NONE);
plot->setInputBindings(scrolling);  // Ctrl + wheel and Shift + wheel still zoom
```

## The plot is slow

Check, in this order: that you are not judging a Debug build; that the large series are sorted by x (the debug
overlay says `pixel-skip` for those that aren't); that data is appended in batches. See
[Performance](performance.md).

## Calls from another thread crash or do nothing

A plot and everything it owns belong to the GUI thread, like every widget. Hand data over with a queued signal or
a timer. See [Data](data.md#live-data).

## Build problems

| Problem | Cause |
| --- | --- |
| Errors inside rocketplot's headers | The compiler is too old or not in C++23 mode. Link the CMake target `rocketplot::rocketplot`, which sets the standard, and check the compiler versions in [Getting started](getting-started.md#what-you-need) |
| Undefined references to `rocketplot::...` | The target doesn't link `rocketplot::rocketplot` |
| `find_package(rocketplot)` fails | The installation's prefix is not in `CMAKE_PREFIX_PATH` |
| `find_package(Qt6)` fails inside rocketplot | Qt's prefix is not in `CMAKE_PREFIX_PATH` (macOS and Windows), or Qt's Svg module is not installed |
| Qt Designer doesn't list the widgets | The plugin was built with another Qt than Designer's. See [Qt Designer](designer.md#when-the-widgets-dont-show-up) |

## Working on this guide

The guide's code is real code. Each chapter's examples are in a source file in [`examples/`](examples)
(`data_examples.cpp` for `data.md`, and so on), compiled into a program, `rocketplot_guide_examples`, that the
project's tests run.

- In a source file, an example is the code between a `// [name]` line and a `// [/name]` line. A name can be
  closed and opened again, to leave lines out of what the guide shows.
- In a chapter, a line `<!-- example: name -->` goes before the code block that shows it, and
  `<!-- output: name -->` before a block of text that an example produces. A C++ block that is not compiled (a
  fragment that needs context) has `<!-- sketch -->` before it.
- The figures in `images/` are drawn by the same examples.

The test `guide_examples` fails when a block in the guide differs from its source, when an example is not shown
anywhere, when a C++ block has no tag, or when a figure is missing. After changing an example, bring the guide up
to date:

```sh
cmake --build --preset clang-debug --target guide   # rewrites the blocks and draws the figures again
ctest --preset clang-debug -R guide_examples        # checks
```

So the way to change a code block is to change its source file, never the Markdown.
