TEMPLATE = subdirs

SUBDIRS = bsatk \
          shared \
          uibase \
          esptk \
          organizer \
          hookdll \
          archive \
          helper \
          plugins \
          nxmhandler \
          BossDummy \
          pythonRunner \
          loot_cli

pythonRunner.depends = uibase
plugins.depends = pythonRunner uibase
hookdll.depends = shared
organizer.depends = shared uibase plugins

CONFIG(debug, debug|release) {
  DESTDIR = $$PWD/../outputd
} else {
  DESTDIR = $$PWD/../output
}

STATICDATAPATH = $${DESTDIR}\\..\\tools\\static_data\\dlls
DLLSPATH = $${DESTDIR}\\dlls

otherlibs.path = $$DLLSPATH
otherlibs.files += $${STATICDATAPATH}\\7z.dll \
    $${BOOSTPATH}\\stage\\lib\\boost_python-vc*-mt-1*.dll

qtlibs.path = $$DLLSPATH

greaterThan(QT_MAJOR_VERSION, 4) {
  QTLIBNAMES += Core Gui Network OpenGL Script Sql Svg Qml Quick Webkit Widgets Xml XmlPatterns
} else {
  QTLIBNAMES += Core Declarative Gui Network OpenGL Script Sql Svg Webkit Xml XmlPatterns
}

QTLIBSUFFIX = $${QT_MAJOR_VERSION}.dll
CONFIG(debug, debug|release): QTLIBSUFFIX = "d$${QTLIBSUFFIX}" # Can't use Debug: .. here, it ignores the line - no idea why, as it works in BossDummy.pro

for(QTNAME, QTLIBNAMES) {
  QTFILE = Qt$${QTNAME}
  qtlibs.files += $$[QT_INSTALL_BINS]\\$${QTFILE}$${QTLIBSUFFIX}
}

INSTALLS += qtlibs otherlibs

OTHER_FILES +=\
    ../SConstruct\
    ../scons_configure.py\
    SConscript
