# rocketplot's licence files: its own (MIT) and, with ROCKETPLOT_BUNDLE_MATH_FONT, those of the math font compiled into
# it (GUST Font License). A program that links rocketplot holds copies of both, and the notices go with them, so they
# are installed also when rocketplot is a subproject (FetchContent, add_subdirectory), where nothing else of it is.
#
# - `cmake --install` puts them in <prefix>/share/doc/rocketplot/ (the font's in latinmodern-math/ under it), in the
#   component rocketplot_licenses. ROCKETPLOT_INSTALL_LICENSES=OFF leaves them out.
# - The target property ROCKETPLOT_LICENSE_FILES of rocketplot lists them (absolute paths), for a project that
#   gathers notices itself: get_target_property(files rocketplot::rocketplot ROCKETPLOT_LICENSE_FILES). An installed
#   package's imported target has it too, with the installed files.

set(rocketplot_license_files ${PROJECT_SOURCE_DIR}/LICENSE)
set(rocketplot_font_license_files)
if(ROCKETPLOT_BUNDLE_MATH_FONT)
    set(rocketplot_font_license_files
        ${PROJECT_SOURCE_DIR}/src/fonts/GUST-FONT-LICENSE.txt
        ${PROJECT_SOURCE_DIR}/src/fonts/README-latinmodern-math.txt)
endif()
set_property(TARGET rocketplot PROPERTY ROCKETPLOT_LICENSE_FILES
    ${rocketplot_license_files} ${rocketplot_font_license_files})

# Where they are installed, relative to the prefix; the package's config file (Install.cmake) names them from this.
# Not CMAKE_INSTALL_DOCDIR: a parent project that included GNUInstallDirs first has set that to its own folder.
set(rocketplot_license_dir ${CMAKE_INSTALL_DATAROOTDIR}/doc/rocketplot)
set(rocketplot_installed_license_files)
if(ROCKETPLOT_INSTALL_LICENSES)
    install(FILES ${rocketplot_license_files}
        DESTINATION ${rocketplot_license_dir}
        COMPONENT rocketplot_licenses)
    foreach(file IN LISTS rocketplot_license_files)
        cmake_path(GET file FILENAME name)
        list(APPEND rocketplot_installed_license_files ${rocketplot_license_dir}/${name})
    endforeach()
    if(rocketplot_font_license_files)
        install(FILES ${rocketplot_font_license_files}
            DESTINATION ${rocketplot_license_dir}/latinmodern-math
            COMPONENT rocketplot_licenses)
        foreach(file IN LISTS rocketplot_font_license_files)
            cmake_path(GET file FILENAME name)
            list(APPEND rocketplot_installed_license_files ${rocketplot_license_dir}/latinmodern-math/${name})
        endforeach()
    endif()
endif()
