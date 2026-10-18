include(GoogleTest)

# Core tests: the Qt-free code, linked straight from its object files (works for static and shared builds).
add_executable(rocketplot_core_tests
    core/AspectTests.cpp
    core/AutoscaleTests.cpp
    core/AxisMappingTests.cpp
    core/AxisTicksTests.cpp
    core/BarsTests.cpp
    core/BoxSummaryTests.cpp
    core/CsvWriterTests.cpp
    core/DecimatorTests.cpp
    core/ErrorDataTests.cpp
    core/ErrorGeometryTests.cpp
    core/FakeMathFont.h
    core/FontFile.h
    core/GlyphSubstitutionsTests.cpp
    core/HistogramTests.cpp
    core/InterpolateTests.cpp
    core/LabelStaggerTests.cpp
    core/LegendFlowTests.cpp
    core/LineBandTests.cpp
    core/MathDump.h
    core/MathLayoutTests.cpp
    core/MathParserTests.cpp
    core/MathPlainTextTests.cpp
    core/MathSegmentsTests.cpp
    core/MathSymbolsTests.cpp
    core/MathTableTests.cpp
    core/MinMaxPyramidTests.cpp
    core/NearestPointTests.cpp
    core/NumberFormatterTests.cpp
    core/OccupancyTests.cpp
    core/OpenTypeReaderTests.cpp
    core/PolylineClipperTests.cpp
    core/RangeTests.cpp
    core/SeriesDataTests.cpp
    core/StretchyGlyphsTests.cpp
    core/TickGeneratorTests.cpp
    core/TimeTicksTests.cpp
    core/Utf8Tests.cpp)
target_link_libraries(rocketplot_core_tests PRIVATE rocketplot_core GTest::gtest_main)
# The math font, read from its file by the tests of the code that reads fonts.
target_compile_definitions(rocketplot_core_tests PRIVATE
    ROCKETPLOT_MATH_FONT_FILE="${PROJECT_SOURCE_DIR}/src/fonts/latinmodern-math.otf")
rocketplot_configure_target(rocketplot_core_tests)

# Widget tests: the public Qt API, on the offscreen platform (see widgets/main.cpp). QTest supplies input helpers
# and QSignalSpy; the assertions are GoogleTest's.
add_executable(rocketplot_tests
    widgets/main.cpp
    widgets/AnnotationTests.cpp
    widgets/AxisTests.cpp
    widgets/BarSeriesTests.cpp
    widgets/BoxSeriesTests.cpp
    widgets/EventMarkerTests.cpp
    widgets/HistogramSeriesTests.cpp
    widgets/IconAnnotationTests.cpp
    widgets/InputBindingsTests.cpp
    widgets/LegendTests.cpp
    widgets/LineSeriesTests.cpp
    widgets/PlotGridTests.cpp
    widgets/PlotLinkTests.cpp
    widgets/PlotWidgetTests.cpp
    widgets/RectAnnotationTests.cpp
    widgets/ReferenceLineTests.cpp
    widgets/RenderedPlotTest.h
    widgets/ScatterSeriesTests.cpp
    widgets/SeriesTests.cpp
    widgets/ShadedSpanTests.cpp
    widgets/TextAnnotationTests.cpp
    widgets/ThemeTests.cpp
    widgets/PlotForm.ui
    widgets/aspect_tests.cpp
    widgets/event_flags_tests.cpp
    widgets/export_tests.cpp
    widgets/form_tests.cpp
    widgets/interaction_tests.cpp
    widgets/legend_entries_tests.cpp
    widgets/outside_legend_tests.cpp
    widgets/math_tests.cpp
    widgets/plottime_tests.cpp
    widgets/state_tests.cpp
    widgets/text_tests.cpp)
# Qt6::Svg: the export tests read an exported SVG drawing back.
target_link_libraries(rocketplot_tests PRIVATE rocketplot::rocketplot Qt6::Svg Qt6::Test GTest::gtest)
set_target_properties(rocketplot_tests PROPERTIES AUTOUIC ON)
target_compile_definitions(rocketplot_tests PRIVATE
    ROCKETPLOT_MATH_FONT_FILE="${PROJECT_SOURCE_DIR}/src/fonts/latinmodern-math.otf")   # form_tests.cpp: PlotForm.ui, a form from Designer
if(NOT ROCKETPLOT_BUNDLE_MATH_FONT)
    target_compile_definitions(rocketplot_tests PRIVATE ROCKETPLOT_NO_MATH_FONT)   # math_tests.cpp: no typesetting
endif()
rocketplot_configure_qt_target(rocketplot_tests)
if(WIN32)
    # Windows finds DLLs on PATH only: put Qt's bin directory there for test discovery and runs (CMake 3.29+).
    set_property(TARGET rocketplot_tests PROPERTY TEST_LAUNCHER
        ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
endif()

# Demo tests: the parts of the demo that are more than a page of example code (its reader of delimited text, the
# telemetry simulator, the property inspector, the data import, the window), linked from rocketplot_demo_lib. With
# the widget tests' main(), and the demo's resources: the pages' source snippets.
if(ROCKETPLOT_BUILD_DEMO)
    add_executable(rocketplot_demo_tests
        widgets/main.cpp
        demo/DataImportWidgetTests.cpp
        demo/DelimitedTextTests.cpp
        demo/MainWindowTests.cpp
        demo/PropertyInspectorTests.cpp
        demo/TelemetrySimulatorTests.cpp
        ${PROJECT_SOURCE_DIR}/demo/demo.qrc)
    target_link_libraries(rocketplot_demo_tests PRIVATE rocketplot_demo_lib Qt6::Test GTest::gtest)
    set_target_properties(rocketplot_demo_tests PROPERTIES AUTORCC ON)
    rocketplot_configure_qt_target(rocketplot_demo_tests)
    if(WIN32)
        set_property(TARGET rocketplot_demo_tests PROPERTY TEST_LAUNCHER
            ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
    endif()
endif()

# Designer plugin tests: what the plugin tells Qt Designer about the widgets, and the plugin itself, loaded from
# its file as Designer loads it.
if(TARGET rocketplot_designer)
    add_executable(rocketplot_designer_tests
        widgets/main.cpp
        designer/CustomWidgetTests.cpp
        designer/plugin_tests.cpp)
    target_link_libraries(rocketplot_designer_tests PRIVATE rocketplot_designer_widgets GTest::gtest)
    target_compile_definitions(rocketplot_designer_tests PRIVATE
        ROCKETPLOT_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/include"
        ROCKETPLOT_DESIGNER_PLUGIN="$<TARGET_FILE:rocketplot_designer>")
    add_dependencies(rocketplot_designer_tests rocketplot_designer)   # loaded, not linked
    rocketplot_configure_qt_target(rocketplot_designer_tests)
    if(WIN32)
        set_property(TARGET rocketplot_designer_tests PROPERTY TEST_LAUNCHER
            ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
    endif()
endif()

# PRE_TEST: list the tests when ctest runs, not at build time (the binary may be sanitized or cross-compiled).
# Labels: `ctest -L core` runs only the Qt-free tests (the tsan preset does).
gtest_discover_tests(rocketplot_core_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS core)
gtest_discover_tests(rocketplot_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS qt)
if(TARGET rocketplot_demo_tests)
    gtest_discover_tests(rocketplot_demo_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS qt)
endif()
if(TARGET rocketplot_designer_tests)
    gtest_discover_tests(rocketplot_designer_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS qt)
endif()
