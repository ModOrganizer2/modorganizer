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
#          pythonRunner \
          esptk# \
#          loot_cli

#plugins.depends = pythonRunner
hookdll.depends = shared
organizer.depends = shared uibase plugins# loot_cli

CONFIG(debug, debug|release) {
    DESTDIR = outputd
} else {
    DESTDIR = output
}
