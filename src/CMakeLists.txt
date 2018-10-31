CMAKE_MINIMUM_REQUIRED (VERSION 2.8.11)

CMAKE_POLICY(SET CMP0020 NEW)
#CMAKE_POLICY(SET CMP0043 NEW)

SET(PROJ_AUTHOR Tannin)
SET(PROJ_ARCH x86)
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(PROJ_ARCH x64)
ENDIF()

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
    downloadlistwidgetcompact.cpp
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
    json.cpp
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
    downloadlistwidgetcompact.h
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
    json.h
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
    downloadlistwidgetcompact.ui
    downloadlistwidget.ui
    credentialsdialog.ui
    categoriesdialog.ui
    activatemodsdialog.ui
    profileinputdialog.ui
    savetextasdialog.ui
    problemsdialog.ui
    previewdialog.ui
    browserdialog.ui
    aboutdialog.ui
  )

SET(organizer_QRCS
    resources.qrc
    stylesheet_resource.qrc
    )

SET(organizer_RCS
    app_icon.rc
    version.rc
	)


SOURCE_GROUP(Source FILES ${organizer_SRCS})
SOURCE_GROUP(Headers FILES ${organizer_HDRS})
SOURCE_GROUP(UI FILES ${organizer_UIS})

# MO projects
SET(default_project_path "${CMAKE_SOURCE_DIR}/..")
GET_FILENAME_COMPONENT(${default_project_path} ${default_project_path} REALPATH)

SET(project_path "${default_project_path}" CACHE PATH "path to the other mo projects")
#TODO this should not be a hardcoded path
SET(lib_path "${project_path}/../../install/libs")

# Qt5
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5QuickWidgets REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
FIND_PACKAGE(Qt5WebEngineWidgets REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5LinguistTools)
QT5_WRAP_UI(organizer_UIHDRS ${organizer_UIS})
QT5_ADD_RESOURCES(organizer_RCCPPS ${organizer_QRCS})
SET(mo_translation_sources ${CMAKE_SOURCE_DIR}/src ${project_path}/uibase/src)
QT5_CREATE_TRANSLATION(organizer_translations_qm ${mo_translation_sources} ${CMAKE_SOURCE_DIR}/src/organizer_en.ts)
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
                    ${project_path}/../usvfs/include
                    ${project_path}/game_gamebryo/src/gamebryo
                    ${project_path}/game_gamebryo/src/creation
                    ${project_path}/game_features/src
                    ${project_path}/githubpp/src
                    ${LZ4_ROOT}/include)

INCLUDE_DIRECTORIES(shared ${ZLIB_INCLUDE_DIRS})
LINK_DIRECTORIES(${lib_path}
                 ${ZLIB_ROOT}/lib
                 ${LZ4_ROOT}/dll)
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

ADD_EXECUTABLE(ModOrganizer WIN32 ${organizer_HDRS} ${organizer_SRCS} ${organizer_UIHDRS} ${organizer_RCS} ${organizer_RCCPPS} ${organizer_translations_qm})
TARGET_LINK_LIBRARIES(ModOrganizer
                      Qt5::Widgets Qt5::WinExtras Qt5::WebEngineWidgets Qt5::Quick
                      Qt5::Qml Qt5::QuickWidgets Qt5::Network
                      ${Boost_LIBRARIES}
                      zlibstatic
                      uibase esptk bsatk githubpp
                      ${usvfs_name}
                      Dbghelp advapi32 Version Shlwapi liblz4)

IF (MSVC)
  SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES COMPILE_FLAGS "/std:c++latest")
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

# use windeployqt.exe to install all required libraries
SET(windeploy_parameters "--no-translations --plugindir qtplugins --libdir dlls --release-with-debug-info --no-compiler-runtime")
INSTALL(
    CODE
    "EXECUTE_PROCESS(COMMAND
        ${qt5bin}/windeployqt.exe ModOrganizer.exe --webenginewidgets ${windeploy_parameters}
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin
    )
    # run it a second time because on the first run it misses some files
    EXECUTE_PROCESS(COMMAND
        ${qt5bin}/windeployqt.exe ModOrganizer.exe --webenginewidgets ${windeploy_parameters}
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin
    )
    EXECUTE_PROCESS(COMMAND
        ${qt5bin}/windeployqt.exe uibase.dll ${windeploy_parameters}
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

install(FILES ${organizer_translations_qm} DESTINATION bin/translations)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stylesheets
                  ${CMAKE_CURRENT_SOURCE_DIR}/tutorials
        DESTINATION bin)
