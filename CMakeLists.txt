CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

ADD_COMPILE_OPTIONS($<$<CXX_COMPILER_ID:MSVC>:/MP> $<$<CXX_COMPILER_ID:MSVC>:$<$<CONFIG:RELEASE>:/O2>> $<$<CXX_COMPILER_ID:MSVC>:$<$<CONFIG:RELWITHDEBINFO>:/O2>>)

PROJECT(organizer)

# sets ModOrganizer as StartUp Project in Visual Studio
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "ModOrganizer")

set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
set(CMAKE_INSTALL_MESSAGE NEVER)

SET(DEPENDENCIES_DIR CACHE PATH "")
# hint to find qt in dependencies path
LIST(APPEND CMAKE_PREFIX_PATH ${QT_ROOT}/lib/cmake)
LIST(APPEND CMAKE_PREFIX_PATH ${LZ4_ROOT}/dll)

ADD_SUBDIRECTORY(src)

set(vcxproj_user_file "${CMAKE_CURRENT_BINARY_DIR}/INSTALL.vcxproj.user")
if(NOT EXISTS ${vcxproj_user_file})
	FILE(WRITE ${vcxproj_user_file}
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<Project ToolsVersion=\"Current\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
			  "<PropertyGroup>\n"
				"<LocalDebuggerWorkingDirectory>${CMAKE_INSTALL_PREFIX}/bin</LocalDebuggerWorkingDirectory>\n"
				"<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>"
				"<LocalDebuggerCommand>${CMAKE_INSTALL_PREFIX}/bin/ModOrganizer.exe</LocalDebuggerCommand>\n"
			  "</PropertyGroup>\n"
			"</Project>\n")
endif()

INSTALL(FILES dump_running_process.bat DESTINATION bin)
