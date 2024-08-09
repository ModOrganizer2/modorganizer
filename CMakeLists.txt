cmake_minimum_required(VERSION 3.16)

# TODO: clean include directives
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)

project(organizer)

# if MO2_INSTALL_IS_BIN is set, this means that we should install directly into the
# installation prefix, without the bin/ subfolder, typically for a standalone build
# to update an existing install
if (MO2_INSTALL_IS_BIN)
	set(_bin ".")
else()
	set(_bin bin)
endif()

add_subdirectory(src)
add_subdirectory(themes)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT organizer)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION ${_bin})
