include(GenerateExportHeader)

# rocketplot_core: the Qt-free part (scales, ticks, number formatting, series storage, decimation). It links no Qt,
# so a Qt include in src/core fails to compile. Its objects are built into the rocketplot library, and the core
# tests link them directly, so its symbols never need exporting, even from a shared library.
add_library(rocketplot_core OBJECT
    core/Autoscale.cpp
    core/AxisMapping.cpp
    core/AxisTicks.cpp
    core/CsvWriter.cpp
    core/Decimator.cpp
    core/ErrorData.cpp
    core/ErrorGeometry.cpp
    core/LabelStagger.cpp
    core/LineBand.cpp
    core/MinMaxPyramid.cpp
    core/NumberFormatter.cpp
    core/Occupancy.cpp
    core/PolylineClipper.cpp
    core/SeriesData.cpp
    core/TickGenerator.cpp
    core/TimeTicks.cpp)
# Core headers are private (src/core/...), but the core may use the Qt-free public value types (Range, UniformX).
target_include_directories(rocketplot_core PUBLIC ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
set_target_properties(rocketplot_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON   # linked into the shared library too
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
rocketplot_configure_target(rocketplot_core)

# rocketplot: the library users link, as rocketplot::rocketplot. Static or shared per ROCKETPLOT_BUILD_SHARED.
if(ROCKETPLOT_BUILD_SHARED)
    set(rocketplot_library_type SHARED)
else()
    set(rocketplot_library_type STATIC)
endif()
add_library(rocketplot ${rocketplot_library_type})
add_library(rocketplot::rocketplot ALIAS rocketplot)

generate_export_header(rocketplot
    BASE_NAME ROCKETPLOT
    EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/rocketplot/export.h)

target_sources(rocketplot
    PRIVATE
        Annotation.cpp
        AnnotationPainter.cpp
        AnnotationPainter.h
        Axis.cpp
        EventMarker.cpp
        InputBindings.cpp
        InteractionController.cpp
        InteractionController.h
        Legend.cpp
        LegendLayout.cpp
        LegendLayout.h
        LineSeries.cpp
        Logging.cpp
        Logging.h
        MarkerPainter.cpp
        MarkerPainter.h
        Overlays.cpp
        Overlays.h
        PlotExport.cpp
        PlotExport.h
        PlotGrid.cpp
        PlotLayout.cpp
        PlotLayout.h
        PlotLink.cpp
        PlotRenderer.cpp
        PlotRenderer.h
        PlotState.cpp
        PlotState.h
        PlotWidget.cpp
        ReferenceLine.cpp
        ScatterSeries.cpp
        Series.cpp
        ShadedSpan.cpp
        TextAnnotation.cpp
        TextPainter.cpp
        TextPainter.h
        Theme.cpp
        ViewHistory.cpp
        ViewHistory.h
        plottime.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Annotation.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Axis.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/EventMarker.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/ExportOptions.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/InputBindings.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Legend.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/LineSeries.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/NumericRange.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/PlotGrid.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/PlotLink.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/PlotWidget.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Range.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/ReferenceLine.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/ScatterSeries.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Series.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/ShadedSpan.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/TextAnnotation.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/Theme.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/UniformX.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/enums.h
            ${PROJECT_SOURCE_DIR}/include/rocketplot/plottime.h
            ${PROJECT_BINARY_DIR}/include/rocketplot/export.h)

target_link_libraries(rocketplot
    PUBLIC Qt6::Widgets
    PRIVATE
        Qt6::Svg                              # only the SVG export uses it: no public header includes it
        $<BUILD_INTERFACE:rocketplot_core>)   # its objects become part of rocketplot; nothing to export
target_compile_features(rocketplot PUBLIC cxx_std_23)
if(NOT ROCKETPLOT_BUILD_SHARED)
    target_compile_definitions(rocketplot PUBLIC ROCKETPLOT_STATIC_DEFINE)
endif()
set_target_properties(rocketplot PROPERTIES
    POSITION_INDEPENDENT_CODE ON   # a static rocketplot can go into a plugin or shared library (the Designer plugin)
    CXX_VISIBILITY_PRESET hidden   # only ROCKETPLOT_EXPORT classes leave the shared library
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
rocketplot_configure_qt_target(rocketplot)
