# rocketplot User Guide

rocketplot is a Qt 6 widget that plots numeric data: lines and scatter points on shared axes, with a legend, from
`std::vector` or any other range of numbers. This guide shows how to use it in an application, from the first
plot to linked plots of millions of points.

It is written for C++ programmers who know the basics of Qt Widgets (a `QApplication`, widgets in layouts, signals
and slots). No knowledge of plotting libraries is assumed.

## Chapters

Read [Getting started](getting-started.md) first. The other chapters stand on their own: go to the one you need.

| Chapter | What it covers |
| --- | --- |
| [Getting started](getting-started.md) | Adding rocketplot to a project, a first program, the parts of a plot and who owns them |
| [Data](data.md) | Copying, moving and viewing data; evenly sampled data; gaps; live data; reading data back |
| [Series](series.md) | Lines and scatter points, markers, a size and color per point, error bars and bands |
| [Axes](axes.md) | Ranges and autoscale, logarithmic and date/time scales, a second y axis, number formats, the grid |
| [Annotations](annotations.md) | Reference lines, shaded spans, text with an arrow, event markers |
| [Interaction](interaction.md) | Pan and zoom, the view history, the crosshair, the context menu, remapping gestures |
| [The legend](legend.md) | Where it goes, what the user can do with it, values at the crosshair |
| [Several plots](multiple-plots.md) | Linking the x axes of stacked plots; a grid of plots as one figure |
| [Appearance](appearance.md) | Light, dark, high-contrast and print themes; a theme of your own; fonts; rich text |
| [Export](export.md) | Images, SVG and PDF drawings, the clipboard, printing, the data as CSV |
| [Saving and restoring state](state.md) | Opening a plot the way the user left it |
| [Qt Designer](designer.md) | The plugin that puts the widgets into Designer's widget box |
| [Performance](performance.md) | What makes large data fast, what makes it slow, and how to measure |
| [Troubleshooting](troubleshooting.md) | Symptoms and their usual causes |

## How to read the examples

The examples leave out what is the same everywhere. They assume

- a `rocketplot::PlotWidget* plot` that already exists (`new rocketplot::PlotWidget(parent)`),
- the headers of the classes they use, which are named after them: `rocketplot::PlotWidget` is in
  `"rocketplot/PlotWidget.h"`, `rocketplot::Axis` in `"rocketplot/Axis.h"`, and so on (the enumerations are all in
  `"rocketplot/enums.h"`),
- data in variables whose names say what they hold (`time`, `altitude`, ...), usually `std::vector<double>`.

Most of the data is from an invented two-stage rocket launch, the kind of thing the library was written for.

The examples in this guide are compiled and run by the project's tests, and the figures are drawn by the same
code: what you read here works with the version of the library it comes with. (The one exception is the fragment
in [Qt Designer](designer.md), which needs a form.) The sources are in
[`examples/`](examples); the last section of [Troubleshooting](troubleshooting.md#working-on-this-guide) explains
how the guide and its examples are kept in step.

## Other documentation

- The [README](../../README.md) has the feature list, the requirements, and how to build the library, its tests
  and its demo.
- The API reference is in the public headers (`include/rocketplot/`), as Doxygen comments. Build it as HTML with
  `cmake --build --preset clang-debug --target docs`.
- The demo application (`rocketplot_demo`) has a page per feature that shows its own source code, and a property
  inspector to try every setting on a live plot.
