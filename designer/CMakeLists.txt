# rocketplot_designer: a plugin for Qt Designer (and Qt Creator's form editor) that puts PlotWidget and PlotGrid
# into the widget box, so that they are placed and sized in a form like Qt's own widgets and their properties show
# in the property editor. Designer finds it in a `designer` folder of a plugin path: Qt's own plugins/designer, or
# a directory named in QT_PLUGIN_PATH (QT_PLUGIN_PATH=build/<preset>/plugins designer6).
#
# The widgets' descriptions are a library of their own so that the tests can link them: a plugin can only be loaded.
add_library(rocketplot_designer_widgets STATIC
    CustomWidget.cpp
    CustomWidget.h)
target_link_libraries(rocketplot_designer_widgets PUBLIC rocketplot::rocketplot Qt6::UiPlugin)
target_include_directories(rocketplot_designer_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rocketplot_designer_widgets PROPERTIES POSITION_INDEPENDENT_CODE ON)   # goes into the plugin
rocketplot_configure_qt_target(rocketplot_designer_widgets)

add_library(rocketplot_designer MODULE
    WidgetCollection.cpp
    WidgetCollection.h)
target_link_libraries(rocketplot_designer PRIVATE rocketplot_designer_widgets)
set_target_properties(rocketplot_designer PROPERTIES
    PREFIX ""   # Qt's plugins have no "lib" in front
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins/designer
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins/designer
    # Installed as <prefix>/plugins/designer/: a shared rocketplot is in <prefix>/lib.
    INSTALL_RPATH "$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
rocketplot_configure_qt_target(rocketplot_designer)

if(ROCKETPLOT_INSTALL)
    install(TARGETS rocketplot_designer
        LIBRARY DESTINATION plugins/designer COMPONENT sdk
        RUNTIME DESTINATION plugins/designer COMPONENT sdk)
endif()
