#-------------------------------------------------
#
# Project created by QtCreator 2011-05-03T18:10:26
#
#-------------------------------------------------


TARGET = ModOrganizer
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4) {
  QT       += core gui widgets network xml sql xmlpatterns qml declarative script webkit webkitwidgets
} else {
  QT       += core gui network xml declarative script sql xmlpatterns webkit
}

!include(../LocalPaths.pri) {
  message("paths to required libraries need to be set up in LocalPaths.pri")
}

SOURCES += \
    transfersavesdialog.cpp \
    syncoverwritedialog.cpp \
    spawn.cpp \
    singleinstance.cpp \
    settingsdialog.cpp \
    settings.cpp \
    selfupdater.cpp \
    selectiondialog.cpp \
    savegameinfowidgetgamebryo.cpp \
    savegameinfowidget.cpp \
    savegamegamebryo.cpp \
    savegame.cpp \
    queryoverwritedialog.cpp \
    profilesdialog.cpp \
    profile.cpp \
    pluginlistsortproxy.cpp \
    pluginlist.cpp \
    overwriteinfodialog.cpp \
    nxmaccessmanager.cpp \
    nexusinterface.cpp \
    motddialog.cpp \
    modlistsortproxy.cpp \
    modlist.cpp \
    modinfodialog.cpp \
    modinfo.cpp \
    messagedialog.cpp \
    mainwindow.cpp \
    main.cpp \
    loghighlighter.cpp \
    logbuffer.cpp \
    lockeddialog.cpp \
    loadmechanism.cpp \
    installationmanager.cpp \
    helper.cpp \
    filedialogmemory.cpp \
    executableslist.cpp \
    editexecutablesdialog.cpp \
    downloadmanager.cpp \
    downloadlistwidgetcompact.cpp \
    downloadlistwidget.cpp \
    downloadlistsortproxy.cpp \
    downloadlist.cpp \
    directoryrefresher.cpp \
    credentialsdialog.cpp \
    categoriesdialog.cpp \
    categories.cpp \
    bbcode.cpp \
    activatemodsdialog.cpp \
    moapplication.cpp \
    profileinputdialog.cpp \
    icondelegate.cpp \
    gameinfoimpl.cpp \
    csvbuilder.cpp \
    savetextasdialog.cpp \
    qtgroupingproxy.cpp \
    modlistview.cpp \
    problemsdialog.cpp \
    serverinfo.cpp \
    browserview.cpp \
    browserdialog.cpp \
    persistentcookiejar.cpp \
    noeditdelegate.cpp \
    previewgenerator.cpp \
    previewdialog.cpp \
    aboutdialog.cpp \
    json.cpp \
    safewritefile.cpp \
    modflagicondelegate.cpp \
    genericicondelegate.cpp \
    organizerproxy.cpp \
    viewmarkingscrollbar.cpp \
    plugincontainer.cpp \
    organizercore.cpp


HEADERS  += \
    transfersavesdialog.h \
    syncoverwritedialog.h \
    spawn.h \
    singleinstance.h \
    settingsdialog.h \
    settings.h \
    selfupdater.h \
    selectiondialog.h \
    savegameinfowidgetgamebryo.h \
    savegameinfowidget.h \
    savegamegamebyro.h \
    savegame.h \
    queryoverwritedialog.h \
    profilesdialog.h \
    profile.h \
    pluginlistsortproxy.h \
    pluginlist.h \
    overwriteinfodialog.h \
    nxmaccessmanager.h \
    nexusinterface.h \
    motddialog.h \
    modlistsortproxy.h \
    modlist.h \
    modinfodialog.h \
    modinfo.h \
    messagedialog.h \
    mainwindow.h \
    loghighlighter.h \
    logbuffer.h \
    lockeddialog.h \
    loadmechanism.h \
    installationmanager.h \
    helper.h \
    filedialogmemory.h \
    executableslist.h \
    editexecutablesdialog.h \
    downloadmanager.h \
    downloadlistwidgetcompact.h \
    downloadlistwidget.h \
    downloadlistsortproxy.h \
    downloadlist.h \
    directoryrefresher.h \
    credentialsdialog.h \
    categoriesdialog.h \
    categories.h \
    bbcode.h \
    activatemodsdialog.h \
    moapplication.h \
    profileinputdialog.h \
    icondelegate.h \
    gameinfoimpl.h \
    csvbuilder.h \
    savetextasdialog.h \
    qtgroupingproxy.h \
    modlistview.h \
    problemsdialog.h \
    serverinfo.h \
    browserview.h \
    browserdialog.h \
    persistentcookiejar.h \
    noeditdelegate.h \
    previewgenerator.h \
    previewdialog.h \
    aboutdialog.h \
    json.h \
    safewritefile.h\
    modflagicondelegate.h \
    genericicondelegate.h \
    organizerproxy.h \
    viewmarkingscrollbar.h \
    plugincontainer.h \
    organizercore.h \
    iuserinterface.h

FORMS    += \
    transfersavesdialog.ui \
    syncoverwritedialog.ui \
    simpleinstalldialog.ui \
    settingsdialog.ui \
    selectiondialog.ui \
    savegameinfowidget.ui \
    queryoverwritedialog.ui \
    profilesdialog.ui \
    overwriteinfodialog.ui \
    motddialog.ui \
    modinfodialog.ui \
    messagedialog.ui \
    mainwindow.ui \
    lockeddialog.ui \
    installdialog.ui \
    finddialog.ui \
    editexecutablesdialog.ui \
    downloadlistwidgetcompact.ui \
    downloadlistwidget.ui \
    credentialsdialog.ui \
    categoriesdialog.ui \
    activatemodsdialog.ui \
    profileinputdialog.ui \
    savetextasdialog.ui \
    problemsdialog.ui \
    previewdialog.ui \
    browserdialog.ui \
    aboutdialog.ui

RESOURCES += \
    resources.qrc \
    stylesheet_resource.qrc

RC_FILE += \
    app_icon.rc

OTHER_FILES += \
    version.rc \
    tutorials/firststeps.qml \
    tutorials/tutorials.js \
    tutorials/tutorial_firststeps_main.js \
    tutorials/tutorials_settingsdialog.qml \
    tutorials/tutorials_mainwindow.qml \
    tutorials/Highlight.qml \
    tutorials/TutorialDescription.qml \
    tutorials/TutorialOverlay.qml \
    tutorials/tutorials_nexusdialog.qml \
    tutorials/tutorials_modinfodialog.qml \
    tutorials/tutorial_firststeps_modinfo.js \
    tutorials/tutorial_conflictresolution_main.js \
    tutorials/tutorial_conflictresolution_modinfo.js \
    app_icon.rc \
    dark.qss \
    stylesheets/dark.qss \
    tutorials/tutorial_window_installer.js \
    tutorials/tutorials_installdialog.qml \
    tutorials/tutorial_firststeps_settings.js


# leak detection with vld
INCLUDEPATH += "E:/Visual Leak Detector/include"
LIBS += -L"E:/Visual Leak Detector/lib/Win32"
#DEFINES += LEAK_CHECK_WITH_VLD

# custom leak detection
#LIBS += -lDbgHelp

# model tests
#SOURCES += modeltest.cpp
#HEADERS += modeltest.h
#DEFINES += TEST_MODELS


INCLUDEPATH += ../shared ../archive ../uibase ../bsatk ../esptk ../plugins/gamefeatures "$${LOOTPATH}" "$${BOOSTPATH}"

LIBS += -L"$${BOOSTPATH}/stage/lib"

CONFIG(debug, debug|release) {
  LIBS += -L$$OUT_PWD/../shared/debug
  LIBS += -L$$OUT_PWD/../bsatk/debug
  LIBS += -L$$OUT_PWD/../esptk/debug
  LIBS += -L$$OUT_PWD/../uibase/debug
  LIBS += -L$$OUT_PWD/../boss_modified/debug
  LIBS += -lDbgHelp
  PRE_TARGETDEPS += \
    $$OUT_PWD/../shared/debug/mo_shared.lib \
    $$OUT_PWD/../bsatk/debug/bsatk.lib \
    $$OUT_PWD/../esptk/debug/esptk.lib
} else {
  LIBS += -L$$OUT_PWD/../shared/release
  LIBS += -L$$OUT_PWD/../bsatk/release
  LIBS += -L$$OUT_PWD/../esptk/release
  LIBS += -L$$OUT_PWD/../uibase/release
  LIBS += -L$$OUT_PWD/../boss_modified/release
  QMAKE_CXXFLAGS += /Zi /GL
  QMAKE_LFLAGS += /DEBUG /LTCG /OPT:REF /OPT:ICF
  PRE_TARGETDEPS += \
    $$OUT_PWD/../shared/release/mo_shared.lib \
    $$OUT_PWD/../bsatk/release/bsatk.lib \
    $$OUT_PWD/../esptk/release/esptk.lib
}

#QMAKE_CXXFLAGS_WARN_ON -= -W3
#QMAKE_CXXFLAGS_WARN_ON += -W4
QMAKE_CXXFLAGS -= -w34100 -w34189
QMAKE_CXXFLAGS += -wd4100 -wd4127 -wd4512 -wd4189

CONFIG += embed_manifest_exe

# QMAKE_CXXFLAGS += /analyze
# QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'highestAvailable\' uiAccess=\'false\'\"

TRANSLATIONS = organizer_en.ts

#!isEmpty(TRANSLATIONS) {
#  isEmpty(QMAKE_LRELEASE) {
#		win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease.exe
#    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
#  }
#
#  isEmpty(TS_DIR):TS_DIR = Translations
#
#  TSQM.name = lrelease ${QMAKE_FILE_IN}
#  TSQM.input = TRANSLATIONS
#  TSQM.output = $$TS_DIR/${QMAKE_FILE_BASE}.qm
#  TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN}
#  TSQM.CONFIG = no_link
#  QMAKE_EXTRA_COMPILERS += TSQM
#  PRE_TARGETDEPS += compiler_TSQM_make_all
#} else:message(No translation files in project)

LIBS += -lmo_shared -luibase -lshell32 -lole32 -luser32 -ladvapi32 -lgdi32 -lPsapi -lVersion -lbsatk -lesptk -lshlwapi

LIBS += -L"$${ZLIBPATH}/build" -lzlibstatic

DEFINES += UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS NOMINMAX PSAPI_VERSION=1

DEFINES += BOOST_DISABLE_ASSERTS NDEBUG QT_MESSAGELOGCONTEXT
#DEFINES += QMLJSDEBUGGER

HGID = 'nothing'
DEFINES += HGID=\\\"$${HGID}\\\"

CONFIG(debug, debug|release) {
  SRCDIR = $$OUT_PWD/debug
  DSTDIR = $$PWD/../../outputd
} else {
  SRCDIR = $$OUT_PWD/release
  DSTDIR = $$PWD/../../output
}

BASEDIR = $$PWD
BASEDIR ~= s,/,$$QMAKE_DIR_SEP,g
SRCDIR ~= s,/,$$QMAKE_DIR_SEP,g
DSTDIR ~= s,/,$$QMAKE_DIR_SEP,g

QMAKE_POST_LINK += xcopy /y /I $$quote($$SRCDIR\\ModOrganizer*.exe) $$quote($$DSTDIR) $$escape_expand(\\n)
QMAKE_POST_LINK += xcopy /y /I $$quote($$SRCDIR\\ModOrganizer*.pdb) $$quote($$DSTDIR) $$escape_expand(\\n)
QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\stylesheets) $$quote($$DSTDIR)\\stylesheets $$escape_expand(\\n)
QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\tutorials) $$quote($$DSTDIR)\\tutorials $$escape_expand(\\n)
QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\*.qm) $$quote($$DSTDIR)\\translations $$escape_expand(\\n)

CONFIG(debug, debug|release) {
  greaterThan(QT_MAJOR_VERSION, 4) {
    QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\..\\dlls.*manifest.debug.qt5) $$quote($$DSTDIR)\\dlls $$escape_expand(\\n)
    QMAKE_POST_LINK += copy /y $$quote($$DSTDIR\\dlls\\dlls.manifest.debug.qt5) $$quote($$DSTDIR\\dlls\\dlls.manifest) $$escape_expand(\\n)
    QMAKE_POST_LINK += del $$quote($$DSTDIR)\\dlls\\dlls.manifest.debug.qt5 $$escape_expand(\\n)
  } else {
    QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\..\\dlls.*manifest.debug) $$quote($$DSTDIR)\\dlls $$escape_expand(\\n)
    QMAKE_POST_LINK += copy /y $$quote($$DSTDIR)\\dlls\\dlls.manifest.debug $$quote($$DSTDIR)\\dlls\\dlls.manifest $$escape_expand(\\n)
    QMAKE_POST_LINK += del $$quote($$DSTDIR)\\dlls\\dlls.manifest.debug $$escape_expand(\\n)
  }
} else {
  greaterThan(QT_MAJOR_VERSION, 4) {
    QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\..\\dlls.*manifest.qt5) $$quote($$DSTDIR)\\dlls $$escape_expand(\\n)
    QMAKE_POST_LINK += copy /y $$quote($$DSTDIR\\dlls\\dlls.manifest.qt5) $$quote($$DSTDIR\\dlls\\dlls.manifest) $$escape_expand(\\n)
    QMAKE_POST_LINK += del $$quote($$DSTDIR)\\dlls\\dlls.manifest.qt5 $$escape_expand(\\n)
  } else {
    QMAKE_POST_LINK += xcopy /y /s /I $$quote($$BASEDIR\\..\\dlls.*manifest) $$quote($$DSTDIR)\\dlls $$escape_expand(\\n)
  }
}

OTHER_FILES += \
    SConscript

DISTFILES += \
    tutorials/tutorial_primer_main.js \
    tutorials/Tooltip.qml \
    tutorials/TooltipArea.qml \
    SConscript
