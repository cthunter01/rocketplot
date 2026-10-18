# A project that uses rocketplot the way the User Guide says to (Getting started): fetched with FetchContent and built
# with the project, which has warnings as errors of its own and, with CONSUMER_SANITIZE, sanitizers of its own. It is
# not part of rocketplot's build: CI configures, builds, runs and installs it on its own (README.md here says how).
cmake_minimum_required(VERSION 3.28)
project(rocketplot_consumer LANGUAGES CXX)

set(ROCKETPLOT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "The rocketplot source tree to build with")
option(CONSUMER_SANITIZE "Build with AddressSanitizer and UBSan (GCC, Clang)" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)   # before rocketplot, as a project would: its CMAKE_INSTALL_DOCDIR is this project's

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

if(CONSUMER_SANITIZE)
    # As the guide says: rocketplot is built with the sanitizers of the project that links it.
    set(ROCKETPLOT_SANITIZERS "address;undefined" CACHE STRING "" FORCE)
endif()
include(FetchContent)
FetchContent_Declare(rocketplot SOURCE_DIR ${ROCKETPLOT_SOURCE_DIR} SYSTEM)
FetchContent_MakeAvailable(rocketplot)

get_target_property(rocketplot_licenses rocketplot::rocketplot ROCKETPLOT_LICENSE_FILES)
message(STATUS "rocketplot's licence files: ${rocketplot_licenses}")
if(NOT rocketplot_licenses)
    message(FATAL_ERROR "rocketplot::rocketplot has no ROCKETPLOT_LICENSE_FILES")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE rocketplot::rocketplot Qt6::Widgets)
target_compile_definitions(consumer PRIVATE QT_NO_KEYWORDS)
if(MSVC)
    target_compile_options(consumer PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
endif()
if(CONSUMER_SANITIZE)
    target_compile_options(consumer PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
                                            -fno-omit-frame-pointer)
    target_link_options(consumer PRIVATE -fsanitize=address,undefined)
    target_compile_definitions(consumer PRIVATE _GLIBCXX_SANITIZE_VECTOR)   # std::vector's spare capacity, too
endif()

install(TARGETS consumer)
