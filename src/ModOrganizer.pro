TEMPLATE = subdirs
CONFIG += ordered


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
    esptk

hookdll.depends = shared
organizer.depends = shared, uibase, plugins

CONFIG(debug, debug|release) {
    DESTDIR = outputd
} else {
    DESTDIR = output
}
