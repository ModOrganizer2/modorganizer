TEMPLATE = subdirs


SUBDIRS = bsatk \
          shared \
          uibase \
          organizer \
          hookdll \
          archive \
          helper \
          plugins \
          proxydll \
          nxmhandler \
          BossDummy \
          pythonRunner \
          boss_modified \
          esptk \
          loot_cli

plugins.depends = pythonRunner
hookdll.depends = shared
organizer.depends = shared uibase plugins boss_modified

CONFIG(debug, debug|release) {
    DESTDIR = outputd
} else {
    DESTDIR = output
}
