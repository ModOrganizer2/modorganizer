CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

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
    savegameinfowidgetgamebryo.cpp
    savegameinfowidget.cpp
    savegamegamebryo.cpp
    savegame.cpp
    queryoverwritedialog.cpp
    profilesdialog.cpp
    profile.cpp
    pluginlistsortproxy.cpp
    pluginlist.cpp
    overwriteinfodialog.cpp
    nxmaccessmanager.cpp
    nexusinterface.cpp
    motddialog.cpp
    modlistsortproxy.cpp
    modlist.cpp
    modinfodialog.cpp
    modinfo.cpp
    messagedialog.cpp
    mainwindow.cpp
    main.cpp
    loghighlighter.cpp
    logbuffer.cpp
    lockeddialog.cpp
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
    gameinfoimpl.cpp
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
    safewritefile.cpp
    modflagicondelegate.cpp
    genericicondelegate.cpp
    organizerproxy.cpp
    viewmarkingscrollbar.cpp
    plugincontainer.cpp
    organizercore.cpp

    shared/inject.cpp
    shared/windows_error.cpp
    shared/error_report.cpp
    shared/directoryentry.cpp
    shared/gameinfo.cpp
    shared/oblivioninfo.cpp
    shared/fallout3info.cpp
    shared/falloutnvinfo.cpp
    shared/util.cpp
    shared/skyriminfo.cpp
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
    savegameinfowidgetgamebryo.h
    savegameinfowidget.h
    savegamegamebyro.h
    savegame.h
    queryoverwritedialog.h
    profilesdialog.h
    profile.h
    pluginlistsortproxy.h
    pluginlist.h
    overwriteinfodialog.h
    nxmaccessmanager.h
    nexusinterface.h
    motddialog.h
    modlistsortproxy.h
    modlist.h
    modinfodialog.h
    modinfo.h
    messagedialog.h
    mainwindow.h
    loghighlighter.h
    logbuffer.h
    lockeddialog.h
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
    gameinfoimpl.h
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
    safewritefile.h
    modflagicondelegate.h
    genericicondelegate.h
    organizerproxy.h
    viewmarkingscrollbar.h
    plugincontainer.h
    organizercore.h
    iuserinterface.h

    shared/inject.h
    shared/windows_error.h
    shared/error_report.h
    shared/directoryentry.h
    shared/gameinfo.h
    shared/oblivioninfo.h
    shared/fallout3info.h
    shared/falloutnvinfo.h
    shared/util.h
    shared/skyriminfo.h
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
    savegameinfowidget.ui
    queryoverwritedialog.ui
    profilesdialog.ui
    overwriteinfodialog.ui
    motddialog.ui
    modinfodialog.ui
    messagedialog.ui
    mainwindow.ui
    lockeddialog.ui
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


# Qt5
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5Declarative REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
FIND_PACKAGE(Qt5WebKitWidgets REQUIRED)
QT5_WRAP_UI(organizer_UIHDRS ${organizer_UIS})
QT5_ADD_RESOURCES(organizer_RCCPPS ${organizer_QRCS})

INCLUDE_DIRECTORIES(${Qt5Declarative_INCLUDES})

ADD_DEFINITIONS(-DQT_MESSAGELOGCONTEXT)

# Boost
SET(Boost_USE_STATIC_LIBS        ON)
SET(Boost_USE_MULTITHREADED      ON)
SET(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost REQUIRED)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})


FIND_PACKAGE(zlib REQUIRED)
# TODO FindZlib doesn't find the static zlib library


# MO projects
SET(default_project_path "${CMAKE_SOURCE_DIR}/..")
GET_FILENAME_COMPONENT(${default_project_path} ${default_project_path} REALPATH)

SET(project_path "${default_project_path}" CACHE PATH "path to the other mo projects")
SET(lib_path "${project_path}/../install/libs")

INCLUDE_DIRECTORIES(${project_path}/uibase/src
                    ${project_path}/bsatk/src
                    ${project_path}/esptk/src
                    ${project_path}/archive/src
                    ${project_path}/plugin/game_features/src)
INCLUDE_DIRECTORIES(shared ${ZLIB_INCLUDE_DIRS})
LINK_DIRECTORIES(${lib_path}
                 ${project_path}/zlib/lib)

ADD_DEFINITIONS(-D_UNICODE -DUNICODE -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)

ADD_EXECUTABLE(ModOrganizer WIN32 ${organizer_HDRS} ${organizer_SRCS} ${organizer_UIHDRS} ${organizer_RCS} ${organizer_RCCPPS})
TARGET_LINK_LIBRARIES(ModOrganizer
                      Qt5::Widgets Qt5::WinExtras Qt5::WebKitWidgets
                      ${Boost_LIBRARIES}
                      zlibstatic
                      uibase esptk bsatk
                      Dbghelp advapi32 Version Shlwapi)

SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES COMPILE_FLAGS /GL)
SET_TARGET_PROPERTIES(ModOrganizer PROPERTIES LINK_FLAGS_RELWITHDEBINFO
                      "/LTCG /INCREMENTAL:NO /LARGEADDRESSAWARE /OPT:REF /OPT:ICF")


QT5_USE_MODULES(ModOrganizer Widgets Declarative Network WebEngineWidgets)


###############
## Installation

SET(qt5bin ${Qt5Core_DIR}/../../../bin)

FIND_PROGRAM(WINDEPLOYQT_COMMAND windeployqt PATHS ${qt5bin} NO_DEFAULT_PATH)

INSTALL(TARGETS ModOrganizer
        RUNTIME DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ModOrganizer.pdb
        DESTINATION pdb)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dlls.manifest.qt5
        DESTINATION bin/dlls
        RENAME dlls.manifest)

# use windeployqt.exe to install all required libraries
SET(windeploy_parameters --no-translations --no-plugins --libdir dlls --release-with-debug-info --no-compiler-runtime)
INSTALL(
    CODE
    "EXECUTE_PROCESS(
        COMMAND
        ${qt5bin}/windeployqt.exe ModOrganizer.exe ${windeploy_parameters}
        COMMAND
        ${qt5bin}/windeployqt.exe uibase.dll ${windeploy_parameters}
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin
    )"
)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stylesheets
                  ${CMAKE_CURRENT_SOURCE_DIR}/tutorials
        DESTINATION bin)
