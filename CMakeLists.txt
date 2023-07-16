cmake_minimum_required(VERSION 3.16)

# TODO: move these to cmake_common?
set(OPENSSL_USE_STATIC_LIBS FALSE CACHE STRING "" FORCE)
set(MySQL_INCLUDE_DIRS CACHE STRING "" FORCE)

if(DEFINED DEPENDENCIES_DIR)
	include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/mo2.cmake)
else()
	include(${CMAKE_CURRENT_LIST_DIR}/../cmake_common/mo2.cmake)
endif()

project(organizer)
add_subdirectory(src)
add_subdirectory(themes)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION bin)
