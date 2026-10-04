# Qt Designer

[User Guide](index.md) · previous: [Saving and restoring state](state.md) · next: [Performance](performance.md)

If you lay out your windows in Qt Designer (`.ui` files), a plugin puts `PlotWidget` and `PlotGrid` into
Designer's widget box. You drag them onto a form like a button, and set their properties in the property editor.

Without the plugin you can still use the widgets in a form; see [Promoting a widget](#promoting-a-widget) below.

## Getting the plugin

The plugin is built with the library when Qt's UiPlugin module is installed. It comes with Qt's tools (the
package is `qt6-tools` on Arch Linux and FreeBSD; the Qt installer's desktop kits include it).

| From | The plugin is at |
| --- | --- |
| A build | `build/<preset>/plugins/designer/rocketplot_designer.so` (`.dll` on Windows) |
| An installation or an `-sdk` release archive | `<prefix>/plugins/designer/` |

When rocketplot is built as part of your project (`FetchContent`, `add_subdirectory()`), the plugin is not built
unless you set `ROCKETPLOT_BUILD_DESIGNER_PLUGIN` to `ON`.

## Letting Designer find it

Designer loads plugins from a folder named `designer` inside each of its plugin paths. Either point it at the
folder that holds the plugin's `designer` folder, or copy the plugin to where Qt keeps its own.

```sh
# Try it from the build: QT_PLUGIN_PATH names the folder that contains designer/
QT_PLUGIN_PATH=build/clang-debug/plugins designer6

# Or keep it with Qt
cp build/clang-debug/plugins/designer/rocketplot_designer.so <Qt>/plugins/designer/
```

**Plot** and **Plot grid** then appear in the widget box under *rocketplot*. In Designer, *Help > About Plugins*
lists the plugins that loaded and says why one didn't.

## Using the widgets in a form

Drag a plot onto the form. The property editor shows its properties, among them the title, the theme mode, the
crosshair, and for a grid the number of rows and columns and which x axes are linked.

What a form can't hold is what a plot shows: the data, the axis labels, the annotations. Those are set in code,
on the widget that the form made:

<!-- sketch -->
```cpp
#include "ui_MainWindow.h"   // generated from MainWindow.ui

MainWindow::MainWindow()
{
    m_ui.setupUi(this);
    // "plot" is the object name the widget has in the form.
    m_ui.plot->xAxis()->setLabel("Time (s)");
    m_ui.plot->addLine(time, altitude, "Altitude");
}
```

The form includes `rocketplot/PlotWidget.h` or `rocketplot/PlotGrid.h` by itself. Your application links
`rocketplot::rocketplot` as usual; the plugin is needed by Designer only, not by the application at run time.

In CMake, turn on `AUTOUIC` for the target so that the `.ui` file is compiled:

```cmake
set_target_properties(my_app PROPERTIES AUTOUIC ON AUTOMOC ON)
target_sources(my_app PRIVATE MainWindow.cpp MainWindow.h MainWindow.ui)
```

## When the widgets don't show up

A Designer plugin is loaded into Designer's own process, so it must fit the Qt that Designer was built with: the
same major version, a version not newer than Designer's, and the same compiler family.

- **Qt Creator's form editor** uses the Qt that Qt Creator itself was built with, which is often not the Qt of
  your kit. A plugin built with your kit's Qt may be refused there. Open the form in the standalone Designer of
  your Qt instead (`designer6`); the form is the same file either way.
- **A Debug plugin in a Release Designer** (or the other way round) is refused on Windows. Build the plugin in
  the configuration of the Designer you run.
- **Nothing in About Plugins at all** means the path is wrong: `QT_PLUGIN_PATH` must name the folder that
  *contains* `designer/`, not the `designer` folder itself.

## Promoting a widget

Without the plugin, put a plain `QWidget` on the form and *promote* it: right-click it, *Promote to…*, and enter

| Field | Value |
| --- | --- |
| Promoted class name | `rocketplot::PlotWidget` |
| Header file | `rocketplot/PlotWidget.h` |

The code that the form generates is the same as with the plugin. What you lose is the preview in Designer (the
widget shows as an empty rectangle) and the properties in the property editor.

Next: [Performance](performance.md).
