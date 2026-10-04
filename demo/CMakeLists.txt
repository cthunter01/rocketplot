# rocketplot_demo_lib: the demo without its main(): the gallery window and its pages, the property inspector, the
# telemetry simulator and the reader of delimited text. A library of its own so that the tests can link it
# (tests/demo/). It is not installed: the demo executable has it inside.
add_library(rocketplot_demo_lib STATIC
    DataImportWidget.cpp
    DataImportWidget.h
    DelimitedText.cpp
    DelimitedText.h
    DemoPage.h
    Generators.cpp
    Generators.h
    MainWindow.cpp
    MainWindow.h
    PropertyInspector.cpp
    PropertyInspector.h
    TelemetrySimulator.cpp
    TelemetrySimulator.h
    pages/AnnotationsPage.cpp
    pages/BasicLinesPage.cpp
    pages/DateTimePage.cpp
    pages/ErrorBarsPage.cpp
    pages/ExportPage.cpp
    pages/GapsPage.cpp
    pages/ImportPage.cpp
    pages/InteractionPage.cpp
    pages/LargeDataPage.cpp
    pages/LegendPage.cpp
    pages/LinkedPlotsPage.cpp
    pages/LiveAppendPage.cpp
    pages/LogScalePage.cpp
    pages/NumberFormatsPage.cpp
    pages/PointStylesPage.cpp
    pages/ScatterPage.cpp
    pages/StatePage.cpp
    pages/TelemetryPage.cpp
    pages/TwoAxesPage.cpp
    pages/UniformSamplingPage.cpp)
target_link_libraries(rocketplot_demo_lib PUBLIC rocketplot::rocketplot)
target_include_directories(rocketplot_demo_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})   # pages/ include "DemoPage.h"
rocketplot_configure_qt_target(rocketplot_demo_lib)

# rocketplot_demo: a gallery of feature pages; each page also shows its own source (embedded through demo.qrc, which
# is compiled into the executable: the linker would drop the resources of a static library nothing refers to).
add_executable(rocketplot_demo
    demo.qrc
    main.cpp)
target_link_libraries(rocketplot_demo PRIVATE rocketplot_demo_lib)
set_target_properties(rocketplot_demo PROPERTIES
    AUTORCC ON
    WIN32_EXECUTABLE ON   # no console window on Windows
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "rocketplot demo"
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.rocketplot.demo
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    # Linux: find a shared rocketplot and a deployed Qt in ../lib after installation.
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
rocketplot_configure_qt_target(rocketplot_demo)

if(NOT ROCKETPLOT_INSTALL)
    return()
endif()

install(TARGETS rocketplot_demo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT demo
    BUNDLE DESTINATION . COMPONENT demo)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT demo)

# Release archives: put the Qt libraries and plugins next to the demo so it runs where Qt isn't installed
# (windeployqt on Windows, macdeployqt into the bundle on macOS, the runtime dependencies into lib/ on Linux).
if(ROCKETPLOT_DEPLOY_QT)
    # On Linux, Qt's deploy script copies every runtime dependency that is not outside the Qt prefix. With a
    # distribution's Qt in /usr that is the whole system, glibc included: deploy only an installer or aqt Qt.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND QT6_INSTALL_PREFIX MATCHES "^/usr/?$")
        message(FATAL_ERROR
            "ROCKETPLOT_DEPLOY_QT needs a Qt from the Qt installer or aqt, not the system Qt in ${QT6_INSTALL_PREFIX}. "
            "The dist-linux preset is meant for CI (see .github/workflows/release.yml).")
    endif()
    qt_generate_deploy_app_script(
        TARGET rocketplot_demo
        OUTPUT_SCRIPT rocketplot_deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME)   # Windows: the VC++ runtime DLLs come from InstallRequiredSystemLibraries below
    install(SCRIPT ${rocketplot_deploy_script} COMPONENT demo)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/QT_LICENSE.txt DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT demo)
    if(WIN32)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT demo)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
        include(InstallRequiredSystemLibraries)
    endif()
endif()
