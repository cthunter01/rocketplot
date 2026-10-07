# Benchmarks (Google Benchmark), in two executables like the tests: the Qt-free core, linked from its object
# files, and the widget through its public API, on the offscreen platform. Build them optimized to measure anything:
#   cmake --workflow --preset bench
#   build/bench/bin/rocketplot_benchmarks --benchmark_filter=Render

add_executable(rocketplot_core_benchmarks
    BenchmarkData.h
    core/DecimatorBenchmarks.cpp
    core/NearestPointBenchmarks.cpp
    core/SeriesDataBenchmarks.cpp
    core/geometry_benchmarks.cpp
    core/text_benchmarks.cpp)
target_link_libraries(rocketplot_core_benchmarks PRIVATE rocketplot_core benchmark::benchmark_main)
target_include_directories(rocketplot_core_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rocketplot_configure_target(rocketplot_core_benchmarks)

add_executable(rocketplot_benchmarks
    BenchmarkData.h
    widgets/data_benchmarks.cpp
    widgets/main.cpp
    widgets/render_benchmarks.cpp)
target_link_libraries(rocketplot_benchmarks PRIVATE rocketplot::rocketplot benchmark::benchmark)
target_include_directories(rocketplot_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rocketplot_configure_qt_target(rocketplot_benchmarks)

# The tests run every benchmark once, so that one that stops working is noticed: the cases of ten million points
# aside, which take a while where the build isn't optimized. Label `bench`.
if(ROCKETPLOT_BUILD_TESTS)
    if(WIN32)
        # Windows finds DLLs on PATH only: put Qt's bin directory there, as for the widget tests.
        set_property(TARGET rocketplot_benchmarks PROPERTY TEST_LAUNCHER
            ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>" --)
    endif()
    foreach(benchmarks IN ITEMS rocketplot_core_benchmarks rocketplot_benchmarks)
        add_test(NAME ${benchmarks}_run_once
            COMMAND ${benchmarks} --benchmark_min_time=1x --benchmark_filter=-10000000)
        set_tests_properties(${benchmarks}_run_once PROPERTIES LABELS bench)
    endforeach()
endif()
