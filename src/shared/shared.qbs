import qbs.base 1.0

Application {
    name: 'Shared'

    Depends { name: 'cpp' }
    Depends { name: 'BSAToolkit' }

    cpp.defines: []
    cpp.libraryPaths: [ qbs.getenv("BOOSTPATH") ]
    cpp.includePaths: [ 'bsatk', qbs.getenv("BOOSTPATH") ]
    files: [
        '*.h',
        '*.cpp'
    ]
}
