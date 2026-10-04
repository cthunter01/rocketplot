# 'docs' target: Doxygen HTML for the public headers, if Doxygen is installed.
#   cmake --build --preset clang-debug --target docs
# Output: build/<preset>/docs/html/index.html. Not part of 'all'.

find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found; the 'docs' target is unavailable")
    return()
endif()

set(DOXYGEN_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/docs)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
set(DOXYGEN_MARKDOWN_ID_STYLE GITHUB)   # the README links to its own headings the way GitHub names them
set(DOXYGEN_IMAGE_PATH ${PROJECT_SOURCE_DIR}/docs/images)   # and shows these
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
set(DOXYGEN_EXTENSION_MAPPING "h=C++")   # headers are .h, but always C++
# Without these, Doxygen reads `class ROCKETPLOT_EXPORT PlotWidget` as a class named ROCKETPLOT_EXPORT.
set(DOXYGEN_ENABLE_PREPROCESSING YES)
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
set(DOXYGEN_PREDEFINED "ROCKETPLOT_EXPORT=" "ROCKETPLOT_NO_EXPORT=" "ROCKETPLOT_DEPRECATED=")
if(DOXYGEN_DOT_FOUND)
    set(DOXYGEN_HAVE_DOT YES)
endif()

doxygen_add_docs(docs
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/README.md
    COMMENT "Generating Doxygen HTML")
