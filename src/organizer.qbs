import qbs.base 1.0

Application {
    name: 'Organizer'

    Depends { name: 'Qt.core' }
    Depends { name: 'Qt.gui' }
    Depends { name: 'Qt.network' }
    Depends { name: 'Qt.declarative' }
    Depends { name: 'UIBase' }
    Depends { name: 'Shared' }
    Depends { name: 'cpp' }

    cpp.defines: []
    cpp.includePaths: [ '../shared', '../archive', '../uibase', qbs.getenv("BOOSTPATH") ]
		// '../bsatk', '../esptk', 

    files: [
        '*.cpp',
        '*.h',
        '*.ui'
    ]
}
