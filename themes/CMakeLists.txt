cmake_minimum_required(VERSION 3.16)

file(GLOB theme_directories
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    LIST_DIRECTORIES TRUE "*")

foreach(theme_directory ${theme_directories})
    if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${theme_directory}")
        file(READ ${theme_directory}/metadata.json JSON_METADATA)
        string(JSON theme_identifier GET ${JSON_METADATA} id)
        install(DIRECTORY ${theme_directory}/ DESTINATION ${MO2_INSTALL_BIN}/extensions/${theme_identifier})
    endif()
endforeach()
