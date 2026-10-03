include(GoogleTest)

# Core tests: the Qt-free code, linked straight from its object files (works for static and shared builds).
add_executable(rocketplot_core_tests
    core/AutoscaleTests.cpp
    core/AxisMappingTests.cpp
    core/AxisTicksTests.cpp
    core/CsvWriterTests.cpp
    core/DecimatorTests.cpp
    core/ErrorDataTests.cpp
    core/ErrorGeometryTests.cpp
    core/LabelStaggerTests.cpp
    core/LineBandTests.cpp
    core/MinMaxPyramidTests.cpp
    core/NumberFormatterTests.cpp
    core/OccupancyTests.cpp
    core/PolylineClipperTests.cpp
    core/RangeTests.cpp
    core/SeriesDataTests.cpp
    core/TickGeneratorTests.cpp
    core/TimeTicksTests.cpp)
target_link_libraries(rocketplot_core_tests PRIVATE rocketplot_core GTest::gtest_main)
rocketplot_configure_target(rocketplot_core_tests)

# Widget tests: the public Qt API, on the offscreen platform (see widgets/main.cpp). QTest supplies input helpers
# and QSignalSpy; the assertions are GoogleTest's.
add_executable(rocketplot_tests
    widgets/main.cpp
    widgets/AnnotationTests.cpp
    widgets/EventMarkerTests.cpp
    widgets/InputBindingsTests.cpp
    widgets/LegendTests.cpp
    widgets/PlotLinkTests.cpp
    widgets/PlotWidgetTests.cpp
    widgets/ReferenceLineTests.cpp
    widgets/RenderedPlotTest.h
    widgets/ScatterSeriesTests.cpp
    widgets/SeriesTests.cpp
    widgets/ShadedSpanTests.cpp
    widgets/TextAnnotationTests.cpp
    widgets/ThemeTests.cpp
    widgets/export_tests.cpp
    widgets/interaction_tests.cpp
    widgets/plottime_tests.cpp)
# Qt6::Svg: the export tests read an exported SVG drawing back.
target_link_libraries(rocketplot_tests PRIVATE rocketplot::rocketplot Qt6::Svg Qt6::Test GTest::gtest)
rocketplot_configure_qt_target(rocketplot_tests)
if(WIN32)
    # Windows finds DLLs on PATH only: put Qt's bin directory there for test discovery and runs (CMake 3.29+).
    set_property(TARGET rocketplot_tests PROPERTY TEST_LAUNCHER
        ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
endif()

# PRE_TEST: list the tests when ctest runs, not at build time (the binary may be sanitized or cross-compiled).
# Labels: `ctest -L core` runs only the Qt-free tests (the tsan preset does).
gtest_discover_tests(rocketplot_core_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS core)
gtest_discover_tests(rocketplot_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS qt)
