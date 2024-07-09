cmake_minimum_required(VERSION 3.16)

# remove MinSizeRel otherwise CMP0111 breaks everything
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)

project(organizer)

add_subdirectory(src)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION bin)
