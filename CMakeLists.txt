cmake_minimum_required(VERSION 3.28)   # FetchContent SYSTEM + EXCLUDE_FROM_ALL + FIND_PACKAGE_ARGS
project(rocketplot VERSION 0.1.0 DESCRIPTION "Qt widget for plotting large numeric data sets" LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset clang-debug")
endif()

# Presets set the build type; this covers a bare `cmake -B build`. A parent project keeps its own choice.
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)     # no C++ modules; remove this line to use them
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option(ROCKETPLOT_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ROCKETPLOT_BUILD_BENCHMARKS "Build the benchmarks (Google Benchmark)" OFF)
option(ROCKETPLOT_BUILD_DOCS "Add a 'docs' target when Doxygen is found" ${PROJECT_IS_TOP_LEVEL})
option(ROCKETPLOT_WARNINGS_AS_ERRORS "Treat compiler and clang-tidy warnings as errors" OFF)
option(ROCKETPLOT_ENABLE_COVERAGE "Clang source-based coverage and a 'coverage' target" OFF)
option(ROCKETPLOT_ENABLE_CLANG_TIDY "Run clang-tidy on our sources while compiling" OFF)
option(ROCKETPLOT_ENABLE_IPO "Link-time optimisation for our targets" OFF)
set(ROCKETPLOT_SANITIZERS "" CACHE STRING "Sanitizers for our targets, e.g. address;undefined or thread")
# The presets set ROCKETPLOT_BUILD_SHARED rather than BUILD_SHARED_LIBS, so FetchContent'd dependencies stay static.
option(ROCKETPLOT_BUILD_SHARED "Build rocketplot as a shared library" ${BUILD_SHARED_LIBS})
option(ROCKETPLOT_BUILD_DEMO "Build the demo application" ${PROJECT_IS_TOP_LEVEL})
option(ROCKETPLOT_BUILD_DESIGNER_PLUGIN "Build the Qt Designer plugin when Qt's UiPlugin module is found"
       ${PROJECT_IS_TOP_LEVEL})
option(ROCKETPLOT_INSTALL "Add install rules (the library's CMake package and the demo)" ${PROJECT_IS_TOP_LEVEL})
option(ROCKETPLOT_DEPLOY_QT "Install the Qt runtime next to the demo (release archives)" OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT CMAKE_CXX_COMPILER_LAUNCHER)   # a launcher the user chose wins
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Point clangd (in any editor) at the most recently configured preset.
# Windows allows symlinks only in Developer Mode (or as admin), so a failure is a note, not an error.
if(PROJECT_IS_TOP_LEVEL)
    file(CREATE_LINK ${PROJECT_BINARY_DIR}/compile_commands.json
         ${PROJECT_SOURCE_DIR}/compile_commands.json RESULT link_result SYMBOLIC)
    if(NOT link_result EQUAL 0)
        message(STATUS "Could not link compile_commands.json into the source tree (${link_result}). "
                       "On Windows, turn on Developer Mode to allow symlinks.")
    endif()
endif()

# Project modules are included by path, so a parent's CMAKE_MODULE_PATH cannot replace them.
include(${PROJECT_SOURCE_DIR}/cmake/ProjectOptions.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake)
include(GNUInstallDirs)

add_subdirectory(src)
if(ROCKETPLOT_BUILD_DEMO)
    add_subdirectory(demo)
endif()
if(ROCKETPLOT_BUILD_DESIGNER_PLUGIN AND TARGET Qt6::UiPlugin)
    add_subdirectory(designer)
endif()
if(ROCKETPLOT_INSTALL)
    include(${PROJECT_SOURCE_DIR}/cmake/Install.cmake)
endif()

if(ROCKETPLOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(ROCKETPLOT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(ROCKETPLOT_ENABLE_COVERAGE)
    include(${PROJECT_SOURCE_DIR}/cmake/Coverage.cmake)
endif()
if(ROCKETPLOT_BUILD_DOCS)
    include(${PROJECT_SOURCE_DIR}/cmake/Docs.cmake)
endif()

# Release archives of what the install() rules install, one per component:
# <project>-<version>-<CPACK_SYSTEM_NAME>-sdk (library, headers, CMake package) and -demo (the demo app).
# Made by the dist-* workflow presets (cmake --workflow --preset dist-linux) and the Release GitHub workflow.
if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL sdk)
    if(ROCKETPLOT_BUILD_DEMO)
        list(APPEND CPACK_COMPONENTS_ALL demo)
    endif()
    set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
    include(CPack)
endif()
