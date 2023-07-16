cmake_minimum_required(VERSION 3.16)

file(GLOB theme_directories
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    LIST_DIRECTORIES TRUE "*")

foreach(theme_directory ${theme_directories})
    if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${theme_directory}")
        install(DIRECTORY ${theme_directory} DESTINATION bin/extensions)
    endif()
endforeach()
