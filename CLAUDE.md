# rocketplot

A Qt 6 Widgets plotting library (`rocketplot::PlotWidget`: many series on shared axes, with a legend, from
`std::vector`/any numeric range, smooth with millions of points) plus a demo app. C++23, CMake presets + Ninja,
GoogleTest. Cross-platform: Linux (GCC, Clang), macOS (Apple Clang), Windows (MSVC) and FreeBSD (Clang).

Needs Qt 6.8+ (Widgets, and Svg for the SVG export; UiPlugin from Qt's tools, `qt6-tools`, for the Designer
plugin, which is skipped without it). Linux and FreeBSD use the system Qt (packages `qt6-base qt6-svg qt6-tools`
on both); on macOS and Windows put Qt's prefix in `CMAKE_PREFIX_PATH` (for example in a `CMakeUserPresets.json`,
which is gitignored). CI builds Windows against Qt 6.8 (the minimum, so newer API is caught) and macOS against 6.10
(Qt 6.8.3 links the AGL framework, which the macOS 26 SDK removed). FreeBSD is built in CI only, in a VM, with the
`ci-clang` preset: Clang with libc++ and the Qt of FreeBSD's packages; the sanitizer, tidy and coverage presets
are not checked there. CI's Arch image has the newest clang-tidy, which may be ahead of this machine's: new checks
can fail the `tidy` job first.
CI's image has no GoogleTest package either, so CI builds GoogleTest from source where this machine links the
installed shared library: `cmake --preset asan -B build/asan-fetched -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`, then
`cmake --build build/asan-fetched` and `ctest --test-dir build/asan-fetched`, is the build CI's `asan` job runs.

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'Decimator\.'` or
  `build/clang-debug/bin/rocketplot_core_tests --gtest_filter='Decimator.*'` (widget tests: `rocketplot_tests`,
  demo tests: `rocketplot_demo_tests`)
- Qt-free tests only: `ctest --preset clang-debug -L core` (widget and demo tests have the label `qt`)
- Demo: `build/clang-debug/bin/rocketplot_demo` (a `.app` bundle on macOS). Screenshots of every gallery page
  without a display: `QT_QPA_PLATFORM=offscreen build/clang-debug/bin/rocketplot_demo --theme light --screenshots <dir>`
  (then look at them; `--inspector` shows the property inspector, `--page <title>` takes one page only, and
  `--settle <ms>` waits longer before each screenshot: the Telemetry page needs 30000 to get past staging)
- Designer plugin: built into `build/<preset>/plugins/designer/`; try it with
  `QT_PLUGIN_PATH=build/clang-debug/plugins designer6`
- Benchmarks: `cmake --workflow --preset bench`, then `build/bench/bin/rocketplot_benchmarks` (the widget) and
  `rocketplot_core_benchmarks` (the core); `--benchmark_filter=<regex>` picks some. Times only mean something on
  an idle machine. The `tidy` and `ci-*` presets build them too and run each once as a test (label `bench`)
- Before finishing a change, also run: `cmake --workflow --preset tidy` (clang-tidy, warnings are errors) and
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan)
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. The pre-commit hook and CI also reject unformatted files

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `bench`, `ci-gcc`, `ci-clang`, and
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`).
Each builds into `build/<preset>/`; never edit anything under `build/`. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux, macOS and FreeBSD; `msvc-*`: Windows);
`cmake --list-presets` shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It tags
`v<project VERSION>` and publishes the `dist-*` archives, so the version is raised in `project()` in
`CMakeLists.txt`. An archive holds what the `install()` rules install: `-sdk` (library, headers, CMake package) and
`-demo` (the demo with the Qt runtime deployed next to it). `dist-linux` refuses a system Qt in `/usr` (deploying it
would copy the whole system), so it only runs in CI, with Qt from install-qt-action.

Library type: `ROCKETPLOT_BUILD_SHARED` (default from `BUILD_SHARED_LIBS`). `ci-clang` and `ci-msvc` build it shared,
everything else static, so CI catches a missing `ROCKETPLOT_EXPORT`.

## Layout
- `include/rocketplot/`: public headers (`PlotWidget`, `Series`/`LineSeries`/`ScatterSeries`, `Axis`, `Legend`,
  `Annotation`/`ReferenceLine`/`ShadedSpan`/`TextAnnotation`/`EventMarker`, `PlotLink`, `PlotGrid` (plots in rows
  and columns: it makes the plots, links each column's x axes with `PlotLink`s and lines the plot areas up through
  `LayoutConstraints`), `InputBindings`, `Theme`,
  `ExportOptions`, `enums.h`, `plottime.h`; Qt-free value types `Range`, `UniformX`, `NumericRange`).
  `export.h` is generated into `build/<preset>/include/rocketplot/`
- `src/core/`: `rocketplot_core`, an OBJECT library with no Qt: series storage (`SeriesData`), min/max pyramid,
  decimation, line band outlines, clipping, errors (`ErrorData` holds the ends of the bars, `ErrorGeometry` turns them
  into bars and band outlines), `LabelStagger` (rows for event flags), `CsvWriter` (the data in view as CSV text),
  axis mapping (linear/log), ticks (`TickGenerator` linear/log, `TimeTicks` calendar), labels and readouts
  (`NumberFormatter`), `AxisTicks` (picks per axis kind), autoscale. Private headers
- `src/`: `rocketplot` (alias `rocketplot::rocketplot`): the widget layer. `PlotLayout` (where everything goes),
  `PlotRenderer` (paints a layout; shared by the screen and the exports), `PlotExport` (the image, SVG and PDF devices
  an export paints on; `PlotWidget::paintExport()` is what they all draw), `InteractionController` (mouse, wheel,
  trackpad and touch input through the `InputBindings`), `ViewHistory` (back/forward), `LegendLayout` (legend
  placement, values and drawing), `Occupancy` (where data was drawn, for the legend's BEST spot), `Overlays`
  (crosshair and zoom box), `AnnotationPainter` (annotations are part of the cached rendering: under the series, over
  them, then their labels), `PlotLink` (linked x axes, crosshair and history), `TextPainter` (plain and rich text),
  `MarkerPainter`, `PlotState` (how settings are written in the JSON of `PlotWidget::saveState()`; each class
  saves and restores its own in private `saveState()`/`restoreState()`: a new setting goes there too),
  `plottime.cpp`, `Logging` (categories `rocketplot.render|input|data`). The legend, crosshair and
  zoom box are drawn over the widget's cached rendering: call `PlotWidget::invalidate()` when what the plot shows
  changes, `update()` for overlays only
- `demo/`: `rocketplot_demo`, a gallery: one `pages/<Name>Page.cpp` per page, listed in `MainWindow.cpp` and
  `demo.qrc` (the code between `// [snippet]` markers is shown in the app). Everything but `main.cpp` is in the
  static library `rocketplot_demo_lib`, so that tests can link it: the window, the pages, and the parts with logic
  of their own: `PropertyInspector` (a tree of every Q_PROPERTY of the page's plots and their parts, through the
  meta-object system: a new property shows up by itself), `TelemetrySimulator` (the flight behind the Telemetry
  page), `DelimitedText` (reads CSV and the like) and `DataImportWidget` (the "Your data" page)
- `designer/`: `rocketplot_designer`, the Qt Designer plugin (a MODULE; `WidgetCollection` is what Designer loads),
  and `rocketplot_designer_widgets`, a static library of what it tells Designer about each widget (`CustomWidget`),
  which `tests/designer/` links. A widget's Q_PROPERTYs are what Designer's property editor shows and what a form
  sets through `set<Property>()`: `tests/widgets/PlotForm.ui` is such a form, compiled by uic for `form_tests.cpp`
- `tests/core/`: `rocketplot_core_tests` (links the core objects); `tests/widgets/`: `rocketplot_tests` (offscreen Qt,
  own `main.cpp`); `tests/demo/`: `rocketplot_demo_tests` (links `rocketplot_demo_lib`, same `main.cpp`; creates
  every gallery page); `tests/designer/`: `rocketplot_designer_tests` (the widget descriptions, and the plugin
  loaded from its file). Class tests: `MyClassTests.cpp`; other tests: `*_tests.cpp`. Tests of what gets drawn use
  the `RenderedPlotTest` fixture (fixed size, theme and axes) and compare a rendering with and without the thing
- `benchmarks/`: Google Benchmark, only with `ROCKETPLOT_BUILD_BENCHMARKS`. `core/`: `rocketplot_core_benchmarks`
  (links the core objects); `widgets/`: `rocketplot_benchmarks` (the public API, offscreen Qt, own `main.cpp`).
  Named like the tests: `MyClassBenchmarks.cpp`, `*_benchmarks.cpp`
- `cmake/ProjectOptions.cmake`: `rocketplot_configure_target()` (warnings, sanitizers, coverage, tidy) and
  `rocketplot_configure_qt_target()` (that plus moc, `QT_NO_KEYWORDS` and a Qt 6.8 deprecation cap)
- `cmake/Dependencies.cmake`: Qt (find_package) and third-party libraries via FetchContent
- `cmake/Install.cmake`, `cmake/rocketplotConfig.cmake.in`: the installed `find_package(rocketplot)` package

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- A class's header and implementation files are named exactly after the class, including capitalization:
  `class MyClass` lives in `include/rocketplot/MyClass.h` and `src/MyClass.cpp`, and its tests in
  `tests/widgets/MyClassTests.cpp`. Core classes: `src/core/MyClass.{h,cpp}`, tests in `tests/core/MyClassTests.cpp`
- Code lives in `namespace rocketplot` (core: `rocketplot::core`, demo: `rocketplot::demo`); project includes use
  quotes: `#include "rocketplot/PlotWidget.h"`, `#include "core/SeriesData.h"`
- `src/core` must not include Qt (it doesn't link Qt, so it won't compile). Public headers never include core headers
- Public classes are marked `ROCKETPLOT_EXPORT` (from `"rocketplot/export.h"`); public templates convert to `double`
  and call exported non-template functions
- Qt code uses `Q_SIGNALS`/`Q_SLOTS`/`Q_EMIT` (`QT_NO_KEYWORDS`). Enumerators are UPPER_CASE (`Marker::CIRCLE`);
  enums meant for properties go in `enums.h` with `Q_ENUM_NS`. Members `m_`, constants `kName`
- Every new target must call `rocketplot_configure_target(<target>)`, or `rocketplot_configure_qt_target()` if it
  uses Qt
- New source files go into the relevant `CMakeLists.txt`; new tests go into `tests/CMakeLists.txt`
- A setting of a plot class is a `Q_PROPERTY` (with `NOTIFY`, and `RESET` when it has a default to go back to):
  the demo's inspector then lists it by itself. One that is part of how a plot is set up, rather than of what it
  shows, also goes into that class's `saveState()`/`restoreState()`
- Widget tests must pass in any order in one process (`build/clang-debug/bin/rocketplot_tests --gtest_shuffle`),
  not only one per process as ctest runs them: follow `QTest::mouseDClick()` on a widget with a `mouseRelease()`
  (QTest otherwise goes on thinking the button is down), and don't rely on where the pointer is when a test starts
- Straight lines along the axes on whole device pixels (grid, axes, ticks, crosshair) are drawn with antialiasing
  off: it changes no pixel of them and takes ten times as long. Everything else is antialiased
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC
- Code must build and pass its tests on Linux, macOS, Windows and FreeBSD (CI runs all four). Use the standard
  library (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it
  in one source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` / `__FreeBSD__` split, with a branch for
  each platform
