CMAKE_MINIMUM_REQUIRED (VERSION 2.8.11)

CMAKE_POLICY(SET CMP0020 NEW)
#CMAKE_POLICY(SET CMP0043 NEW)

SET(PROJ_AUTHOR Tannin)
SET(PROJ_ARCH x86)
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(PROJ_ARCH x64)
ENDIF()

macro(add_msvc_precompiled_header PrecompiledHeader PrecompiledSource SourcesVar HeadersVar)
  if(MSVC)
    get_filename_component(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
    set(Sources ${${SourcesVar}})

    set_source_files_properties(
		${PrecompiledSource} PROPERTIES
		COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
		OBJECT_OUTPUTS "${PrecompiledBinary}")

    set_source_files_properties(
		${Sources} PROPERTIES
		COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
		OBJECT_DEPENDS "${PrecompiledBinary}")

    list(APPEND ${SourcesVar} ${PrecompiledSource})
	list(APPEND ${HeadersVar} ${PrecompiledHeader})
  endif(MSVC)
endmacro(add_msvc_precompiled_header)


SET(organizer_SRCS
    transfersavesdialog.cpp
    syncoverwritedialog.cpp
    spawn.cpp
    singleinstance.cpp
    settingsdialog.cpp
    settings.cpp
    selfupdater.cpp
    selectiondialog.cpp
    queryoverwritedialog.cpp
    profilesdialog.cpp
    profile.cpp
    pluginlistsortproxy.cpp
    pluginlist.cpp
    pluginlistview.cpp
    overwriteinfodialog.cpp
    nxmaccessmanager.cpp
    nexusinterface.cpp
    motddialog.cpp
    modlistsortproxy.cpp
    modlist.cpp
    modidlineedit.cpp
    modinfodialog.cpp
	modinfodialogcategories.cpp
	modinfodialogconflicts.cpp
	modinfodialogesps.cpp
	modinfodialogimages.cpp
	modinfodialogtab.cpp
	modinfodialogtextfiles.cpp
    modinfo.cpp
    modinfobackup.cpp
    modinfoforeign.cpp
    modinfooverwrite.cpp
    modinforegular.cpp
    modinfoseparator.cpp
    modinfowithconflictinfo.cpp
    messagedialog.cpp
    mainwindow.cpp
    main.cpp
    loghighlighter.cpp
    logbuffer.cpp
    lockeddialogbase.cpp
    lockeddialog.cpp
    waitingonclosedialog.cpp
    loadmechanism.cpp
    installationmanager.cpp
    helper.cpp
    filedialogmemory.cpp
    executableslist.cpp
    editexecutablesdialog.cpp
    downloadmanager.cpp
    downloadlistwidget.cpp
    downloadlistsortproxy.cpp
    downloadlist.cpp
    directoryrefresher.cpp
    credentialsdialog.cpp
    categoriesdialog.cpp
    categories.cpp
    bbcode.cpp
    activatemodsdialog.cpp
    moapplication.cpp
    profileinputdialog.cpp
    icondelegate.cpp
    csvbuilder.cpp
    savetextasdialog.cpp
    qtgroupingproxy.cpp
    modlistview.cpp
    problemsdialog.cpp
    serverinfo.cpp
    browserview.cpp
    browserdialog.cpp
    persistentcookiejar.cpp
    noeditdelegate.cpp
    previewgenerator.cpp
    previewdialog.cpp
    aboutdialog.cpp
    modflagicondelegate.cpp
    genericicondelegate.cpp
    organizerproxy.cpp
    viewmarkingscrollbar.cpp
    plugincontainer.cpp
    organizercore.cpp
    instancemanager.cpp
    usvfsconnector.cpp
    eventfilter.cpp
    moshortcut.cpp
    listdialog.cpp
    lcdnumber.cpp
    forcedloaddialog.cpp
    forcedloaddialogwidget.cpp
	filterwidget.cpp
	statusbar.cpp
	apiuseraccount.cpp
	filerenamer.cpp
	texteditor.cpp
	expanderwidget.cpp

    shared/windows_error.cpp
    shared/error_report.cpp
    shared/directoryentry.cpp
    shared/util.cpp
    shared/appconfig.cpp
    shared/leaktrace.cpp
    shared/stackdata.cpp
  )

SET(organizer_HDRS
    transfersavesdialog.h
    syncoverwritedialog.h
    spawn.h
    singleinstance.h
    settingsdialog.h
    settings.h
    selfupdater.h
    selectiondialog.h
    queryoverwritedialog.h
    profilesdialog.h
    profile.h
    pluginlistsortproxy.h
    pluginlist.h
    pluginlistview.h
    overwriteinfodialog.h
    nxmaccessmanager.h
    nexusinterface.h
    motddialog.h
    modlistsortproxy.h
    modlist.h
    modidlineedit.h
    modinfodialog.h
	modinfodialogcategories.h
	modinfodialogconflicts.h
	modinfodialogesps.h
	modinfodialogimages.h
	modinfodialogtab.h
	modinfodialogtextfiles.h
    modinfo.h
    modinfobackup.h
    modinfoforeign.h
    modinfooverwrite.h
    modinforegular.h
    modinfoseparator.h
    modinfowithconflictinfo.h
    messagedialog.h
    mainwindow.h
    loghighlighter.h
    logbuffer.h
    lockeddialogbase.h
    lockeddialog.h
    waitingonclosedialog.h
    loadmechanism.h
    installationmanager.h
    helper.h
    filedialogmemory.h
    executableslist.h
    editexecutablesdialog.h
    downloadmanager.h
    downloadlistwidget.h
    downloadlistsortproxy.h
    downloadlist.h
    directoryrefresher.h
    credentialsdialog.h
    categoriesdialog.h
    categories.h
    bbcode.h
    activatemodsdialog.h
    moapplication.h
    profileinputdialog.h
    icondelegate.h
    csvbuilder.h
    savetextasdialog.h
    qtgroupingproxy.h
    modlistview.h
    problemsdialog.h
    serverinfo.h
    browserview.h
    browserdialog.h
    persistentcookiejar.h
    noeditdelegate.h
    previewgenerator.h
    previewdialog.h
    aboutdialog.h
    modflagicondelegate.h
    genericicondelegate.h
    organizerproxy.h
    viewmarkingscrollbar.h
    plugincontainer.h
    organizercore.h
    ilockedwaitingforprocess.h
    iuserinterface.h
    instancemanager.h
    usvfsconnector.h
    eventfilter.h
    descriptionpage.h
    moshortcut.h
    listdialog.h
    lcdnumber.h
    forcedloaddialog.h
    forcedloaddialogwidget.h
	filterwidget.h
	statusbar.h
	apiuseraccount.h
	filerenamer.h
	texteditor.h
	expanderwidget.h

    shared/windows_error.h
    shared/error_report.h
    shared/directoryentry.h
    shared/util.h
    shared/appconfig.h
    shared/appconfig.inc
    shared/leaktrace.h
    shared/stackdata.h
  )

SET(organizer_UIS
    transfersavesdialog.ui
    syncoverwritedialog.ui
    simpleinstalldialog.ui
    settingsdialog.ui
    selectiondialog.ui
    queryoverwritedialog.ui
    profilesdialog.ui
    overwriteinfodialog.ui
    motddialog.ui
    modinfodialog.ui
    messagedialog.ui
    mainwindow.ui
    lockeddialog.ui
    waitingonclosedialog.ui
    installdialog.ui
    finddialog.ui
    editexecutablesdialog.ui
    credentialsdialog.ui
    categoriesdialog.ui
    activatemodsdialog.ui
    profileinputdialog.ui
    savetextasdialog.ui
    problemsdialog.ui
    previewdialog.ui
    browserdialog.ui
    aboutdialog.ui
    listdialog.ui
    forcedloaddialog.ui
    forcedloaddialogwidget.ui
  )

SET(organizer_QRCS
    resources.qrc
    stylesheet_resource.qrc
    )

SET(organizer_RCS
    app_icon.rc
    version.rc
	)


source_group(src REGULAR_EXPRESSION ".*\\.(h|cpp)")

set(application
	iuserinterface
	main
	mainwindow
	moapplication
	moshortcut
	selfupdater
	singleinstance
	statusbar
)

set(browser
	browserdialog
	browserview
)

set(core
	categories
	shared/directoryentry
	directoryrefresher
	installationmanager
	instancemanager
	loadmechanism
	nexusinterface
	nxmaccessmanager
	organizercore
	organizerproxy
	apiuseraccount
)

set(dialogs
	aboutdialog
	activatemodsdialog
	categoriesdialog
	credentialsdialog
	filedialogmemory
	forcedloaddialog
	forcedloaddialogwidget
	listdialog
	messagedialog
	motddialog
	overwriteinfodialog
	queryoverwritedialog
	problemsdialog
	savetextasdialog
	selectiondialog
	syncoverwritedialog
	transfersavesdialog
	waitingonclosedialog
)

set(downloads
	downloadlist
	downloadlistsortproxy
	downloadlistwidget
	downloadmanager
)

set(executables
	executableslist
	editexecutablesdialog
)

set(locking
	ilockedwaitingforprocess
	lockeddialog
	lockeddialogbase
)

set(modinfo
	modinfo
	modinfobackup
	modinfodialog
	modinfodialogcategories
	modinfodialogconflicts
	modinfodialogesps
	modinfodialogimages
	modinfodialogtab
	modinfodialogtextfiles
	modinfoforeign
	modinfooverwrite
	modinforegular
	modinfoseparator
	modinfowithconflictinfo
)

set(modlist
	modlist
	modlistsortproxy
	modlistview
)

set(plugins
	plugincontainer
	pluginlist
	pluginlistsortproxy
	pluginlistview
)

set(previews
	previewdialog
	previewgenerator
)

set(profiles
	profile
	profileinputdialog
	profilesdialog
)

set(settings
	settings
	settingsdialog
)

set(utilities
	shared/appconfig
	bbcode
	csvbuilder
	shared/error_report
	eventfilter
	helper
	shared/leaktrace
	persistentcookiejar
	serverinfo
	spawn
	shared/stackdata
	shared/util
	usvfsconnector
	shared/windows_error
)

set(widgets
	descriptionpage
	expanderwidget
	genericicondelegate
	filerenamer
	filterwidget
	icondelegate
	lcdnumber
	logbuffer
	loghighlighter
	modflagicondelegate
	modidlineedit
	noeditdelegate
	qtgroupingproxy
	texteditor
	viewmarkingscrollbar
)

set(src_filters
	application core browser dialogs downloads executables locking modinfo
	modlist plugins previews profiles settings utilities widgets
)

foreach(filter in list ${src_filters})
	set(files)
	foreach(d in lists ${${filter}})
		set(files ${files} ${d}.cpp ${d}.h ${d}.inc)
	endforeach()

	source_group(src\\${filter} FILES ${files})
endforeach()

file(GLOB_RECURSE rule_files ../vsbuild/CMakeFiles/*.rule)
file(GLOB qm_files ../vsbuild/src/*.qm)

source_group(cmake FILES CMakeLists.txt)
source_group(autogen FILES ${rule_files} ${qm_files})
source_group(resources FILES ${organizer_RCS} ${organizer_QRCS})

# MO projects
SET(dependency_project_path "${DEPENDENCIES_DIR}")
SET(default_project_path "${DEPENDENCIES_DIR}/modorganizer_super")
GET_FILENAME_COMPONENT(${default_project_path} ${default_project_path} REALPATH)
SET(project_path "${default_project_path}" CACHE PATH "path to the other mo projects")

add_msvc_precompiled_header("pch.h" "pch.cpp" organizer_SRCS organizer_HDRS)

#TODO this should not be a hardcoded path
SET(lib_path "${project_path}/../../install/libs")

# Qt5
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
SET(CMAKE_AUTORCC ON)

FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5QuickWidgets REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
FIND_PACKAGE(Qt5WebEngineWidgets REQUIRED)
FIND_PACKAGE(Qt5WebSockets REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5LinguistTools)

SET(mo_translation_sources ${CMAKE_SOURCE_DIR}/src ${project_path}/uibase/src)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP autogen)
set_property(GLOBAL PROPERTY AUTOMOC_SOURCE_GROUP autogen)
set_property(GLOBAL PROPERTY AUTORCC_SOURCE_GROUP autogen)

QT5_CREATE_TRANSLATION(organizer_translations_qm ${mo_translation_sources} ${CMAKE_SOURCE_DIR}/src/organizer_en.ts OPTIONS -silent)
ADD_CUSTOM_TARGET(translations DEPENDS ${organizer_translations_qm})

INCLUDE_DIRECTORIES(${Qt5Declarative_INCLUDES})

ADD_DEFINITIONS(-DQT_MESSAGELOGCONTEXT)

# Boost
SET(Boost_USE_STATIC_LIBS       OFF)
SET(Boost_USE_MULTITHREADED      ON)
SET(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost REQUIRED)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})


FIND_PACKAGE(zlib REQUIRED)
# TODO FindZlib doesn't find the static zlib library

INCLUDE_DIRECTORIES(${project_path}/uibase/src
                    ${project_path}/bsatk/src
                    ${project_path}/esptk/src
                    ${project_path}/archive/src
                    ${dependency_project_path}/usvfs/include
                    ${project_path}/game_gamebryo/src/gamebryo
                    ${project_path}/game_gamebryo/src/creation
                    ${project_path}/game_features/src
                    ${project_path}/githubpp/src
                    ${LZ4_ROOT}/lib)

INCLUDE_DIRECTORIES(shared ${ZLIB_INCLUDE_DIRS})
LINK_DIRECTORIES(${lib_path}
                 ${ZLIB_ROOT}/lib
                 ${LZ4_ROOT}/bin)
EXECUTE_PROCESS(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

ADD_DEFINITIONS(-D_UNICODE -DUNICODE -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE -DGITID="${GIT_COMMIT_HASH}")

IF("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  SET(usvfs_name usvfs_x64)
ELSE()
  SET(usvfs_name usvfs_x86)
ENDIF()

ADD_EXECUTABLE(ModOrganizer WIN32 ${organizer_HDRS} ${organizer_SRCS} ${organizer_RCS} ${organizer_QRCS} ${organizer_translations_qm})
TARGET_LINK_LIBRARIES(ModOrganizer
                      Qt5::Widgets Qt5::WinExtras Qt5::WebEngineWidgets Qt5::Quick
                      Qt5::Qml Qt5::QuickWidgets Qt5::Network Qt5::WebSockets
                      ${Boost_LIBRARIES}
                      zlibstatic
                      uibase esptk bsatk githubpp
                      ${usvfs_name}
                      Dbghelp advapi32 Version Shlwapi liblz4 Crypt32)

IF (MSVC)
  SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES COMPILE_FLAGS "/std:c++latest /permissive-")
ENDIF()
IF (MSVC AND "${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
  # 32 bits
  SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES LINK_FLAGS "/LARGEADDRESSAWARE")
ENDIF()

IF (NOT "${OPTIMIZE_COMPILE_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES COMPILE_FLAGS_RELWITHDEBINFO ${OPTIMIZE_COMPILE_FLAGS})
ENDIF()
IF (NOT "${OPTIMIZE_LINK_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES LINK_FLAGS_RELWITHDEBINFO ${OPTIMIZE_LINK_FLAGS})
ENDIF()


###############
## Installation

SET(qt5bin ${Qt5Core_DIR}/../../../bin)

FIND_PROGRAM(WINDEPLOYQT_COMMAND windeployqt PATHS ${qt5bin} NO_DEFAULT_PATH)

INSTALL(TARGETS ModOrganizer
        RUNTIME DESTINATION bin)
INSTALL(FILES $<TARGET_PDB_FILE:ModOrganizer>
        DESTINATION pdb)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest.qt5
        DESTINATION bin/dlls
        RENAME dlls.manifest)

INSTALL(FILES ${organizer_translations_qm} DESTINATION bin/translations)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stylesheets
                  ${CMAKE_CURRENT_SOURCE_DIR}/tutorials
        DESTINATION bin)

#We should do this last, since windeployqt.exe scans the folders in bin for required modules e.g QtQuick.2 required by tutorials.
# use windeployqt.exe to install all required libraries
SET(windeploy_parameters "--no-translations --plugindir qtplugins --libdir dlls --release-with-debug-info --no-compiler-runtime")
INSTALL(
    CODE
    "EXECUTE_PROCESS(COMMAND
        ${qt5bin}/windeployqt.exe --verbose 0 ModOrganizer.exe --webenginewidgets --websockets ${windeploy_parameters}
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin
    )
    EXECUTE_PROCESS(COMMAND
        ${qt5bin}/windeployqt.exe --verbose 0 uibase.dll ${windeploy_parameters}
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin
    )
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/bin/platforms)
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/bin/styles)
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/bin/dlls/imageformats)
    file(RENAME ${CMAKE_INSTALL_PREFIX}/bin/qtplugins/platforms ${CMAKE_INSTALL_PREFIX}/bin/platforms)
    file(RENAME ${CMAKE_INSTALL_PREFIX}/bin/qtplugins/styles ${CMAKE_INSTALL_PREFIX}/bin/styles)
    file(RENAME ${CMAKE_INSTALL_PREFIX}/bin/qtplugins/imageformats ${CMAKE_INSTALL_PREFIX}/bin/dlls/imageformats)
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/bin/qtplugins)"
)

# qdds.dll needs installing manually as Qt no longer ships with it by default.
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../qdds.dll DESTINATION bin/dlls/imageformats)