import qbs.base 1.0

Application {
    name: 'Organizer'

    Depends { name: "Qt"; submodules: ["core", "gui", "network", "declarative"] }
    Depends { name: 'Shared' }
    Depends { name: 'UIBase' }
    Depends { name: 'cpp' }

    cpp.defines: [ 'UNICODE', '_UNICODE' ]
    cpp.libraryPaths: [ qbs.getenv('BOOSTPATH') + '/stage/lib' ]
    cpp.includePaths: [ '../shared', '../archive', '../bsatk', '../esptk', '../uibase', qbs.getenv("BOOSTPATH") ]
		// '../bsatk', '../esptk', 

    cpp.staticLibraries: [ 'shell32', 'user32', 'Version', 'shlwapi' ]
    //LIBS += -lmo_shared -luibase -lshell32 -lole32 -luser32 -ladvapi32 -lgdi32 -lPsapi -lVersion -lbsatk -lshlwapi

    Group {
        name: 'Headers'
        files: [ '*.h' ]
    }

    Group {
        name: 'Sources'
        files: [ '*.cpp' ]
    }

    Group {
        name: 'UI Files'
        files: [ '*.ui' ]
    }

    Group {
        name: 'ESP Toolkit'
        files: [ '../esptk/*.h', '../esptk/*.cpp' ]
    }
}

// /nologo /c


// /Zi -GR -W3
