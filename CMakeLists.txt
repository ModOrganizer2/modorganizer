cmake_minimum_required(VERSION 3.16)

project(organizer)

add_subdirectory(src)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION bin)
