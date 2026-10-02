# rocketplot

A Qt 6 Widgets plotting library (`rocketplot::PlotWidget`: many series on shared axes, with a legend, from
`std::vector`/any numeric range, smooth with millions of points) plus a demo app. C++23, CMake presets + Ninja,
GoogleTest. Cross-platform: Linux (GCC, Clang), macOS (Apple Clang) and Windows (MSVC).

Needs Qt 6.8+. Linux uses the system Qt; on macOS and Windows put Qt's prefix in `CMAKE_PREFIX_PATH` (for example in
a `CMakeUserPresets.json`, which is gitignored). CI builds Windows against Qt 6.8 (the minimum, so newer API is caught)
and macOS against 6.10 (Qt 6.8.3 links the AGL framework, which the macOS 26 SDK removed). CI's Arch image has the
newest clang-tidy, which may be ahead of this machine's: new checks can fail the `tidy` job first.

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'Decimator\.'` or
  `build/clang-debug/bin/rocketplot_core_tests --gtest_filter='Decimator.*'` (widget tests: `rocketplot_tests`)
- Qt-free tests only: `ctest --preset clang-debug -L core` (widget tests have the label `qt`)
- Demo: `build/clang-debug/bin/rocketplot_demo` (a `.app` bundle on macOS). Screenshots of every gallery page
  without a display: `QT_QPA_PLATFORM=offscreen build/clang-debug/bin/rocketplot_demo --theme light --screenshots <dir>`
  (then look at them)
- Before finishing a change, also run: `cmake --workflow --preset tidy` (clang-tidy, warnings are errors) and
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan)
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. The pre-commit hook and CI also reject unformatted files

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`, and
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`).
Each builds into `build/<preset>/`; never edit anything under `build/`. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets`
shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It tags
`v<project VERSION>` and publishes the `dist-*` archives, so the version is raised in `project()` in
`CMakeLists.txt`. An archive holds what the `install()` rules install: `-sdk` (library, headers, CMake package) and
`-demo` (the demo with the Qt runtime deployed next to it). `dist-linux` refuses a system Qt in `/usr` (deploying it
would copy the whole system), so it only runs in CI, with Qt from install-qt-action.

Library type: `ROCKETPLOT_BUILD_SHARED` (default from `BUILD_SHARED_LIBS`). `ci-clang` and `ci-msvc` build it shared,
everything else static, so CI catches a missing `ROCKETPLOT_EXPORT`.

## Layout
- `include/rocketplot/`: public headers (`PlotWidget`, `Series`/`LineSeries`/`ScatterSeries`, `Axis`, `Legend`,
  `PlotLink`, `InputBindings`, `Theme`, `enums.h`, `plottime.h`; Qt-free value types `Range`, `UniformX`,
  `NumericRange`).
  `export.h` is generated into `build/<preset>/include/rocketplot/`
- `src/core/`: `rocketplot_core`, an OBJECT library with no Qt: series storage (`SeriesData`), min/max pyramid,
  decimation, line band outlines, clipping, axis mapping (linear/log), ticks (`TickGenerator` linear/log,
  `TimeTicks` calendar), labels and readouts (`NumberFormatter`), `AxisTicks` (picks per axis kind), autoscale.
  Private headers
- `src/`: `rocketplot` (alias `rocketplot::rocketplot`): the widget layer. `PlotLayout` (where everything goes),
  `PlotRenderer` (paints a layout; shared by screen and future exports), `InteractionController` (mouse, wheel,
  trackpad and touch input through the `InputBindings`), `ViewHistory` (back/forward), `Overlays` (crosshair and
  zoom box, drawn over the widget's cached rendering), `PlotLink` (linked x axes, crosshair and history),
  `TextPainter` (plain and rich text), `MarkerPainter`, `plottime.cpp`, `Logging` (categories
  `rocketplot.render|input|data`)
- `demo/`: `rocketplot_demo`, a gallery: one `pages/<Name>Page.cpp` per page, listed in `MainWindow.cpp` and
  `demo.qrc` (the code between `// [snippet]` markers is shown in the app)
- `tests/core/`: `rocketplot_core_tests` (links the core objects); `tests/widgets/`: `rocketplot_tests` (offscreen Qt,
  own `main.cpp`). Class tests: `MyClassTests.cpp`; other tests: `*_tests.cpp`
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
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC
- Code must build and pass its tests on Linux, macOS and Windows (CI runs all three). Use the standard library
  (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it in one
  source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a branch for each platform
