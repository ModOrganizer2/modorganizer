cmake_minimum_required(VERSION 3.16)

project(organizer)
set(project_type exe)
set(executable_name ModOrganizer)

include(../cmake_common/project.cmake)

set(additional_translations ${uibase_path}/src)

add_subdirectory(src)

install(FILES dump_running_process.bat DESTINATION bin)
