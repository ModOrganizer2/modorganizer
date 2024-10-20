cmake_minimum_required(VERSION 3.16)

# TODO: clean include directives
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)

project(organizer)

find_package(mo2-cmake CONFIG REQUIRED)

set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)

add_subdirectory(src)
add_subdirectory(themes)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT organizer)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION ${MO2_INSTALL_BIN})
