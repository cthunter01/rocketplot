# rocketplot

A Qt 6 widget for plotting numeric data in C++ applications: any number of data sets on shared axes, with a
legend, taken straight from `std::vector` (or any range of numbers), and smooth to pan and zoom with millions of
points per series. A demo application shows off what it does and is where new features get worked out.

```cpp
#include "rocketplot/Axis.h"
#include "rocketplot/PlotWidget.h"

auto* plot = new rocketplot::PlotWidget(parent);
plot->setTitle("Ascent");
plot->xAxis()->setLabel("Time (s)");
plot->yAxis()->setLabel("Altitude (km)");

std::vector<double> time = ..., stage1 = ..., stage2 = ...;
plot->addLine(time, stage1, "Stage 1");   // copied, as double, from any sized range of numbers
plot->addLine(time, stage2, "Stage 2");

// A uniformly sampled channel needs no x array: x = start + i * step.
plot->addLine(rocketplot::UniformX{.start = 0.0, .step = 1.0 / 1000.0}, samples, "Accelerometer (1 kHz)");
```

- **Data**: copied in from any range of numbers (`int`, `float`, `std::int16_t`, ...), moved in from an rvalue
  `std::vector<double>`, or plotted in place without a copy (`addLineView`). `append()` adds live data.
  NaN and infinite values are gaps.
- **Large data**: sorted series are drawn from their visible range only, as the extremes of each pixel column,
  read from a precomputed min/max pyramid. 10 million points pan and zoom at interactive frame rates.
- **Interaction**: drag to pan, wheel to zoom about the pointer (over an axis: only that axis; Ctrl: x only, Shift:
  y only), double-click to autoscale again.
- **Look**: light and dark themes that follow the application's palette, a colorblind-safe series palette,
  hairline grid, tabular tick labels. A debug overlay shows layout boxes, frame time and what each series drew.
- MIT licensed; needs only Qt.

Run the demo to see it: `build/clang-debug/bin/rocketplot_demo` after building.

### Use it in your project
Install it (`cmake --install build/<preset> --prefix <dir>`) or download the `-sdk` release archive, then:
```cmake
find_package(rocketplot 0.1 REQUIRED)   # with <dir> in CMAKE_PREFIX_PATH
target_link_libraries(my_app PRIVATE rocketplot::rocketplot)
```
Or build it with your project through `FetchContent` or `add_subdirectory()`.

## Requirements
- Qt 6.8 or later (Widgets). On Linux, from the distribution (Arch: `qt6-base`); on macOS and Windows, from the
  [Qt online installer](https://www.qt.io/download-qt-installer) or [aqt](https://github.com/miurahr/aqtinstall),
  with its prefix in `CMAKE_PREFIX_PATH` (e.g. in a `CMakeUserPresets.json`)
- CMake 3.28+ and Ninja
- A C++23 compiler with `<print>`:
  - Linux: GCC 14+ or Clang 18+
  - macOS: Xcode 16.3+ or its Command Line Tools (Apple Clang 17+)
  - Windows: Visual Studio 2022 17.7+ (MSVC) with the "Desktop development with C++" workload
- Optional: clang-tidy, clang-format, llvm-cov/llvm-profdata (coverage), Doxygen (docs), ccache.
  On macOS, clang-tidy comes from Homebrew (`brew install llvm`). Coverage uses Xcode's llvm-cov.

GoogleTest is used from the system when installed, otherwise downloaded at configure time.

## Build
Linux and macOS:
```sh
cmake --workflow --preset dev          # configure + build + test, Clang Debug
./build/clang-debug/bin/rocketplot_demo
```

Windows, from a **Developer PowerShell for VS** (Ninja needs MSVC's environment; VS Code's CMake Tools and
Visual Studio set it up themselves):
```powershell
cmake --workflow --preset dev-msvc     # configure + build + test, MSVC Debug
.\build\msvc-debug\bin\rocketplot_demo.exe   # with Qt's bin directory on PATH
```

The demo can also save a screenshot of every page and quit, without a display:
`QT_QPA_PLATFORM=offscreen ./build/clang-debug/bin/rocketplot_demo --theme dark --screenshots shots/`.

| Preset | Platforms | What it is |
| --- | --- | --- |
| `clang-debug`, `clang-release` | Linux, macOS | Everyday builds (Apple Clang on macOS) |
| `gcc-debug`, `gcc-release` | Linux | Everyday builds |
| `msvc-debug`, `msvc-release` | Windows | Everyday builds |
| `asan` | Linux, macOS | Clang Debug with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Linux, macOS | Clang RelWithDebInfo with ThreadSanitizer |
| `tidy` | Linux, macOS | Clang Debug running clang-tidy on every file; findings are errors |
| `coverage` | Linux, macOS | `cmake --workflow --preset coverage` writes `build/coverage/coverage/html/index.html` |
| `ci-gcc`, `ci-clang`, `ci-msvc` | as their compiler | Release builds with warnings as errors, as run in CI (`ci-clang` and `ci-msvc` build rocketplot as a shared library) |
| `dist-linux`, `dist-macos`, `dist-windows` | Linux, macOS, Windows | The release archives (see [Releases](#releases)) |

A preset exists only on the platforms it supports; `cmake --list-presets` shows the ones for this machine.
Each workflow preset (`dev`, `dev-msvc`, `ci-gcc`, `ci-clang`, `ci-msvc`, `asan`, `tsan`, `tidy`, `coverage`)
configures, builds and tests in one command, and the `dist-*` ones also package. Separate steps:
`cmake --preset <p>`, `cmake --build --preset <p>`, `ctest --preset <p>`.

CI (GitHub Actions) builds and tests on all three: Linux (`ci-gcc`, `ci-clang`, `asan`, `tidy`), macOS
(`ci-clang`) and Windows (`ci-msvc`).

API docs: `cmake --build --preset clang-debug --target docs`, then open `build/clang-debug/docs/html/index.html`.

## Releases
The Release workflow (`.github/workflows/release.yml`) runs only when started by hand, never on a push:
1. Raise `VERSION` in `project()` in `CMakeLists.txt`, then commit and push.
2. Start it from the Actions tab (Release > Run workflow, pick the branch) or with `gh workflow run release.yml`
   (`-f prerelease=true` marks it a pre-release).

It stops at once if the tag `v<version>` already exists. Otherwise it runs all of CI and builds, tests and
packages an archive on each platform. Only when every job passes does it tag the commit `v<version>` and
publish a GitHub release with the archives, a `SHA256SUMS` file and generated release notes.

Each platform gets two archives: `rocketplot-<version>-<platform>-sdk` (the static library, headers and CMake
package, for `find_package(rocketplot)`) and `rocketplot-<version>-<platform>-demo` (the demo, with the Qt
libraries and plugins it needs next to it).

| Platform | Built with | Runs on |
| --- | --- | --- |
| `linux-x86_64` (`.tar.gz`) | GCC 14, Ubuntu 24.04, Qt 6.8 | x86-64 Linux with glibc 2.39+ (Ubuntu 24.04+, Debian 13+, Fedora 40+, RHEL 10+). The demo also needs OpenGL, fontconfig and `libxcb-cursor0` |
| `macos-universal` (`.tar.gz`) | Apple Clang, Qt 6.8 | macOS 14+, Apple silicon and Intel |
| `windows-x86_64` (`.zip`) | MSVC, Qt 6.8 | 64-bit Windows (the VC++ runtime DLLs are included) |

Qt is used under the LGPL-3.0 (see `QT_LICENSE.txt` in the demo archive). Other dependencies are built from source,
never taken from the build machine. An archive holds what the `install()` rules install.
Build one with `cmake --workflow --preset dist-macos` (or `dist-windows`); it lands in `build/dist-<os>/package/`.
`dist-linux` needs a Qt from the Qt installer or aqt: it refuses a distribution's Qt in `/usr`, which can't be
deployed (it would copy the whole system), so on Arch it runs in CI only.

The executables are not code-signed. A macOS browser download needs
`xattr -d com.apple.quarantine rocketplot_demo.app` before it runs, and Windows SmartScreen warns the first time.

## License
MIT; see [LICENSE](LICENSE).
