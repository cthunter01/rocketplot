include(FetchContent)

# FIND_PACKAGE_ARGS: an installed package (find_package) wins; otherwise the source is downloaded.
# SYSTEM: the dependency's headers are system headers, so our warnings and clang-tidy skip them.
# EXCLUDE_FROM_ALL: only the parts of the dependency we link against get built.

# Qt is never downloaded: it comes from the system (Linux packages) or an installer/aqt, found through
# CMAKE_PREFIX_PATH (CI sets it; locally on macOS and Windows put it in a CMakeUserPresets.json).
set(rocketplot_qt_components Widgets Svg)   # Svg: QSvgGenerator, for PlotWidget::exportSvg()
if(ROCKETPLOT_BUILD_TESTS)
    list(APPEND rocketplot_qt_components Test)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${rocketplot_qt_components})

# The Designer plugin needs Qt's UiPlugin module, which comes with Qt's tools (Arch: qt6-tools; part of what the Qt
# installer and aqt install by default). Without it there is no plugin, and nothing else changes.
if(ROCKETPLOT_BUILD_DESIGNER_PLUGIN)
    find_package(Qt6 6.8 QUIET COMPONENTS UiPlugin)
    if(NOT TARGET Qt6::UiPlugin)
        message(STATUS "Qt's UiPlugin module was not found: the Qt Designer plugin is not built")
    endif()
endif()

# rocketplot_sanitize_fetched(<target>...)
# A sanitized build sanitizes the test and benchmark libraries too, where they are built here from downloaded
# source. They handle the same vector types as the code that uses them (std::vector<std::string>, for one), and
# libstdc++'s vector annotations for AddressSanitizer must be on in all of that code or in none (see
# rocketplot_sanitize_target()): with GoogleTest left out, a test that reserved a vector of strings made
# AddressSanitizer report a container-overflow inside GoogleTest. An installed library (an imported target, with
# its own compiled code) is used as it is.
function(rocketplot_sanitize_fetched)
    foreach(target IN LISTS ARGN)
        if(TARGET ${target})
            get_target_property(imported ${target} IMPORTED)
            if(NOT imported)
                rocketplot_sanitize_target(${target})
            endif()
        endif()
    endforeach()
endfunction()

if(ROCKETPLOT_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    # MSVC: link the same C runtime as our targets (CMAKE_MSVC_RUNTIME_LIBRARY: the DLL one unless a preset
    # says otherwise) instead of GoogleTest's static default.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
        FIND_PACKAGE_ARGS NAMES GTest)
    FetchContent_MakeAvailable(googletest)
    rocketplot_sanitize_fetched(gtest gtest_main gmock gmock_main)
endif()

if(ROCKETPLOT_BUILD_BENCHMARKS)
    # Only the library: not Google Benchmark's own tests, nor install rules for it.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.5
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
        FIND_PACKAGE_ARGS)
    FetchContent_MakeAvailable(benchmark)
    rocketplot_sanitize_fetched(benchmark benchmark_main)
endif()

# Adding another dependency (then link fmt::fmt):
#
# FetchContent_Declare(fmt
#     GIT_REPOSITORY https://github.com/fmtlib/fmt.git
#     GIT_TAG        12.2.0
#     GIT_SHALLOW    TRUE
#     SYSTEM
#     EXCLUDE_FROM_ALL
#     FIND_PACKAGE_ARGS)
# FetchContent_MakeAvailable(fmt)
