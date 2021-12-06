cmake_minimum_required(VERSION 3.16)

project(organizer)
set(project_type exe)
set(executable_name ModOrganizer)
set(enable_warnings OFF)

set(OPENSSL_USE_STATIC_LIBS FALSE CACHE STRING "" FORCE)
set(MySQL_INCLUDE_DIRS CACHE STRING "" FORCE)

# appveyor does not build modorganizer in its standard location, so use
# DEPENDENCIES_DIR to find cmake_common
if(DEFINED DEPENDENCIES_DIR)
	include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/project.cmake)
else()
	include(../cmake_common/project.cmake)
endif()

set(additional_translations ${uibase_path}/src)

add_subdirectory(src)

install(FILES dump_running_process.bat DESTINATION bin)
