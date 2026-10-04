# The examples of the user guide (docs/user-guide). Every block of C++ the guide shows is code that is compiled
# here: the code between a `// [name]` and a `// [/name]` line of one of these sources. rocketplot_guide_examples
# runs the examples, draws the guide's figures and compares the guide's blocks with the sources (see main.cpp).
add_executable(rocketplot_guide_examples
    Examples.cpp
    Examples.h
    GuideChecker.cpp
    GuideChecker.h
    SampleData.cpp
    SampleData.h
    annotations_examples.cpp
    appearance_examples.cpp
    axes_examples.cpp
    data_examples.cpp
    export_examples.cpp
    getting_started_examples.cpp
    interaction_examples.cpp
    legend_examples.cpp
    main.cpp
    multiple_plots_examples.cpp
    performance_examples.cpp
    series_examples.cpp
    state_examples.cpp)
target_link_libraries(rocketplot_guide_examples PRIVATE rocketplot::rocketplot)
target_compile_definitions(rocketplot_guide_examples PRIVATE
    ROCKETPLOT_GUIDE_DIR="${PROJECT_SOURCE_DIR}/docs/user-guide")
rocketplot_configure_qt_target(rocketplot_guide_examples)

# The guide's first program is a whole one, with a main() of its own: built to see that it compiles.
add_executable(rocketplot_guide_first_plot first_plot.cpp)
target_link_libraries(rocketplot_guide_first_plot PRIVATE rocketplot::rocketplot)
rocketplot_configure_qt_target(rocketplot_guide_first_plot)

# Windows finds DLLs on PATH only: put Qt's bin directory there for the runs below (as for the tests).
set(guide_launcher "")
if(WIN32)
    set(guide_launcher ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
endif()

# The test: every example runs, every figure draws, and the guide shows the examples as they are.
add_test(NAME guide_examples COMMAND ${guide_launcher} $<TARGET_FILE:rocketplot_guide_examples> --check)
set_tests_properties(guide_examples PROPERTIES LABELS qt)

# After changing an example: rewrites the guide's blocks and draws its figures again, in the source tree.
#   cmake --build --preset clang-debug --target guide
add_custom_target(guide
    COMMAND ${guide_launcher} $<TARGET_FILE:rocketplot_guide_examples> --update
            --figures ${PROJECT_SOURCE_DIR}/docs/user-guide/images
    COMMENT "Rewriting the user guide's code blocks and figures from its examples"
    VERBATIM)
