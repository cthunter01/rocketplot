# Install rules for the library's CMake package (component sdk). The demo adds its own (component demo).
# Users then do: find_package(rocketplot) and target_link_libraries(app PRIVATE rocketplot::rocketplot).

include(CMakePackageConfigHelpers)

set(rocketplot_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/rocketplot)

install(TARGETS rocketplot EXPORT rocketplotTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT sdk
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT sdk NAMELINK_COMPONENT sdk
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT sdk
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT sdk)
install(EXPORT rocketplotTargets
    NAMESPACE rocketplot::
    DESTINATION ${rocketplot_cmake_dir}
    COMPONENT sdk)

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/rocketplotConfig.cmake.in
    ${PROJECT_BINARY_DIR}/rocketplotConfig.cmake
    INSTALL_DESTINATION ${rocketplot_cmake_dir})
# 0.x: a minor version may break the API, so only the same minor version is compatible.
write_basic_package_version_file(${PROJECT_BINARY_DIR}/rocketplotConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
        ${PROJECT_BINARY_DIR}/rocketplotConfig.cmake
        ${PROJECT_BINARY_DIR}/rocketplotConfigVersion.cmake
    DESTINATION ${rocketplot_cmake_dir}
    COMPONENT sdk)

install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT sdk)
