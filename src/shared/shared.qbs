import qbs.base 1.0

StaticLibrary {
    name: {
        print(qbs.getenv("BOOSTPATH") + "/stage/lib")
        return 'Shared'
    }

    Depends { name: 'cpp' }
    Depends { name: 'BSAToolkit' }

    cpp.defines: []
    cpp.libraryPaths: [ qbs.getenv("BOOSTPATH") + "/stage/lib" ]
    cpp.includePaths: [ '../bsatk', qbs.getenv("BOOSTPATH") ]
    files: [
        '*.h',
        '*.cpp',
        '*.inc'
    ]
}
