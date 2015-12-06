import distutils.sysconfig
import os
import re
import sys

def setup_config_variables():
    """ Set up defaults values and load the configuration settings """
    # Take a sensible default for pythonpath
    must_be_specified = 'Must be specified ***'

    boostpath = must_be_specified
    if 'BOOSTPATH' in os.environ:
        boostpath = os.environ['BOOSTPATH']

    lootpath = must_be_specified
    if 'LOOTPATH' in os.environ:
        lootpath = os.environ['LOOTPATH']

    pythonpath = must_be_specified
    if 'PYTHONPATH' in os.environ:
        pythonpath = os.environ['PYTHONPATH']
    else:
        pythonpath = distutils.sysconfig.EXEC_PREFIX

    # Take qtdir from the os environment if set
    qtdir = must_be_specified
    if 'QTDIR' in os.environ:
        qtdir = os.environ['QTDIR']
    elif 'QT4DIR' in os.environ:
        qtdir = os.environ['QT4DIR']

    sevenzippath = must_be_specified
    if 'SEVENZIPPATH' in os.environ:
        sevenzippath = os.environ['SEVENZIPPATH']

    zlibpath = must_be_specified
    if 'ZLIBPATH' in os.environ:
        zlibpath = os.environ['ZLIBPATH']

    git = 'git'
    if 'GIT' in os.environ:
        git = os.environ['GIT']

    mercurial = 'hg'
    if 'MERCURIAL' in os.environ:
        hg = os.environ['HG']

    vars = Variables('scons_configure.py')
    vars.AddVariables(
        PathVariable('BOOSTPATH', 'Set to point to your boost directory',
                                             boostpath, PathVariable.PathIsDir),
        PathVariable('LOOTPATH', 'Set to point to your LOOT API directory',
                                              lootpath, PathVariable.PathIsDir),
        ('MSVC_VERSION', 'Version of msvc, defaults to latest installed'),
        PathVariable('PYTHONPATH', 'Path to python install', pythonpath,
                                                        PathVariable.PathIsDir),
        PathVariable('QTDIR', 'Path to the version of QT to use', qtdir,
                                                        PathVariable.PathIsDir),
        PathVariable('SEVENZIPPATH', 'Path to 7zip sources', sevenzippath,
                                                        PathVariable.PathIsDir),
        PathVariable('ZLIBPATH', 'Path to zlib install', zlibpath,
                                                        PathVariable.PathIsDir),
        PathVariable('GIT', 'Path to git executable', git,
                                                       PathVariable.PathIsFile),
        PathVariable('MERCURIAL', 'Path to hg executable', mercurial,
                                                       PathVariable.PathIsFile),
        PathVariable('IWYU', 'Path to include-what-you-use executable', None,
                                                        PathVariable.PathIsFile)
    )

    return vars

def add_moc_files(self, files):
    """
    The QT4 tool only sets up moc for .cpp files. If there's a header file
    with no corresponding source we have to do it ourselves. This makes it
    easier.
    Note: I could just supress the scanning and moc all the HEADER files...
    """
    targets = []
    for file in self.Flatten([files]):
        contents = file.get_contents()
        target = str(file)[:-1] + 'cpp'
        if not os.path.exists(target):
            if 'Q_OBJECT' in file.get_contents():
                header = file.name
                target = self['QT5_XMOCHPREFIX'] + header[:-2] + \
                                                         self['QT5_XMOCHSUFFIX']
                targets.append(self.ExplicitMoc5(target, header))
    return targets

def link_emitter_wrapper(target, source, env):
    # This could be better written!
    if '/DEBUG' in env['LINKFLAGS']:
        name = str(target[0])[:-3] + 'pdb'
        target.append(name)
    return (target, source)

def shlib_emitter_wrapper(target, source, env):
    if 'WINDOWS_EMBED_MANIFEST' in env:
        env.AppendUnique(LINKFLAGS = '/MANIFEST')
    return link_emitter_wrapper(target, source, env)

def fixup_qt4():
    import SCons.Tool
    oldpath = sys.path
    sys.path = SCons.Tool.DefaultToolpath + sys.path
    import qt4
    def my_qrc_path(head, prefix, tail, suffix):
        return "%s%s%s" % (prefix, tail, suffix)
    qt4.__qrc_path = my_qrc_path
    sys.path = oldpath

metadata = re.compile(r'Q_PLUGIN_METADATA\(IID *".*" *FILE *"(.*)"\)')

def json_emitter(target, source, env):
    depends = []
    for s in source:
        match = metadata.search(s.get_contents())
        if match:
            depends += [ match.group(1) ]
    env.Depends(target, depends)
    return target, source

def fixup_qt5():
    import SCons.Tool
    oldpath = sys.path
    sys.path = SCons.Tool.DefaultToolpath + sys.path
    import qt5
    def my_qrc_path(head, prefix, tail, suffix):
        return "%s%s%s" % (prefix, tail, suffix)
    qt5.__qrc_path = my_qrc_path
    sys.path = oldpath

    qt_env['BUILDERS']['Moc5'].builder.emitter = json_emitter

moduleDefines = {
    '3Support' : [ 'QT_QT3SUPPORT_LIB', 'QT3_SUPPORT' ],
    'Core'     : [ 'QT_CORE_LIB' ],
    'Declarative' : [ 'QT_DECLARATIVE_LIB' ],
    'Gui'      : [ 'QT_GUI_LIB' ],
    'Network'  : [ 'QT_NETWORK_LIB' ],
    'OpenGL'   : [ 'QT_OPENGL_LIB' ],
    'Script'   : [ 'QT_SCRIPT_LIB' ],
    'Sql'      : [ 'QT_SQL_LIB' ],
    'Svg'      : [ 'QT_SVG_LIB' ],
    'WebKit'   : [ 'QT_WEBKIT_LIB' ],
    'Xml'      : [ 'QT_XML_LIB' ],
    'XmlPatterns' : [ 'QT_XMLPATTERNS_LIB' ],
# qt5
    'Qml'      : [ 'QT_QML_LIB' ],
    'QuickWidgets' : [ 'QT_QUICKWIDGETS_LIB', 'QT_WIDGETS_LIB', 'QT_QUICK_LIB' ],
    'Widgets'  : [ 'QT_WIDGETS_LIB' ],
    'WebKitWidgets' : [ 'QT_WEBKITWIDGETS_LIB' ],
    'WinExtras' : [ 'QT_WINEXTRAS_LIB' ]
}

staticModules = [
    'UiTools',
]

def get_qt_lib_info(env):
    " Deal with the various QT naming conventions. sigh "

    # For QT4, the libraries are QTlibname{,d}4
    # For QT5, they are QT5libname{,d}

    prefix = 'QT'
    suffix = 'd' if env['CONFIG'] == 'debug' else ''

    _suffix = suffix # Because they can't be consistent within a version,
                     # let alone between
    version = env['QT_MAJOR_VERSION']
    if version <= 4:
        suffix += str(version)
    else:
        prefix += str(version)
    return prefix, suffix, _suffix

def EnableQtModules(self, *modules):
    """ Enable the specified QT modules, mainly by adding defines
        and libraries
    """
    self.AppendUnique(QT_USED_MODULES = modules)
    for module in modules:
        try:
            self.AppendUnique(CPPDEFINES = moduleDefines[module])
            if self['CONFIG'] == 'debug':
                if module == 'Declarative':
                    self.AppendUnique(CPPDEFINES = 'QT_DECLARATIVE_DEBUG')
                elif module == 'Qml':
                    self.AppendUnique(CPPDEFINES = 'QT_QML_DEBUG')
        except:
            print 'module', module, 'has no -D'
            pass

    if "Assistant" in modules:
        self.AppendUnique(CPPPATH = [
            os.path.join('$QTDIR', 'include', 'QtAssistant')
        ])
        modules.remove('Assistant')
        modules.append('AssistantClient')

    prefix, suffix, _suffix = get_qt_lib_info(self)
    self.AppendUnique(LIBS = [
        prefix + lib + suffix for lib in modules if lib not in staticModules
    ])

    self.PrependUnique(LIBS = [
        lib + _suffix for lib in modules if lib in staticModules
    ])

    if 'OpenGL' in modules:
        self.AppendUnique(LIBS = [ 'opengl32' ])

    self.AppendUnique(CPPPATH = [
        os.path.join('$QTDIR', 'include', 'QT' + module) for module in modules
    ])

def DisableQtModules(self, *modules):
    """ Disable the specified QT modules similar to enabling them """
    for module in modules:
        try:
            self['QT_USED_MODULES'].remove(module)
            self['CPPDEFINES'].remove(moduleDefines[module])
            if self['CONFIG'] == 'debug' and module == 'Declarative':
                self['CPPDEFINES'].remove('QT_DECLARATIVE_DEBUG')
        except:
            pass


    if "Assistant" in modules:
        self['CPPPATH'].remove(os.path.join('$QTDIR', 'include', 'QtAssistant'))
        modules.remove('Assistant')
        modules.append('AssistantClient')

    prefix, suffix, _suffix = get_qt_lib_info(self)

    for lib in modules:
        self['LIBS'].remove(prefix + lib + suffix)

    if 'OpenGL' in modules:
        self['LIBS'].remove('opengl32')

    for module in modules:
        self['CPPPATH'].remove(os.path.join('$QTDIR', 'include', 'QT' + module))

def setup_IWYU(env):
    import SCons.Defaults
    import SCons.Builder
    original_shared = SCons.Defaults.SharedObjectEmitter
    original_static = SCons.Defaults.StaticObjectEmitter

    def DoIWYU(env, source, target):
        for i in range(len(source)):
            s = source[i]
            dir, name = os.path.split(str(s)) # I'm sure theres a way of getting this from scons
            # Don't bother looking at moc files and 7zip source
            if not name.startswith('moc_') and \
               not dir.startswith(env['SEVENZIPPATH']):
                # Put the .iwyu in the same place as the .obj
                targ = os.path.splitext(str(target[i]))[0]
                env.Depends(env.IWYU(targ + '.iwyu', s), target[i])

    def shared_emitter(target, source, env):
        DoIWYU(env, source, target)
        return original_shared(target, source, env)

    def static_emitter(target, source, env):
        DoIWYU(env, source, target)
        return original_static(target, source, env)

    SCons.Defaults.SharedObjectEmitter = shared_emitter
    SCons.Defaults.StaticObjectEmitter = static_emitter

    def emitter(target, source, env):
        env.Depends(target, env['IWYU_MAPPING_FILE'])
        env.Depends(target, env['IWYU_MASSAGE'])
        return target, source

    def _concat_list(prefixes, list, suffixes, env, f=lambda x: x, target=None, source=None):
        """ Creates a new list from 'list' by first interpolating each element
            in the list using the 'env' dictionary and then calling f on the
            list, and concatenate the 'prefix' and 'suffix' LISTS onto each element of the list.
            A trailing space on the last element of 'prefix' or leading space on the
            first element of 'suffix' will cause them to be put into separate list
            elements rather than being concatenated.
        """

        if not list:
            return list

        l = f(SCons.PathList.PathList(list).subst_path(env, target, source))
        if l is not None:
            list = l

        # This bit replaces current concat_ixes

        result = []

        def process_stringlist(s):
            return [ str(env.subst(p, SCons.Subst.SUBST_RAW))
                                              for p in Flatten([s]) if p != '' ]

        # ensure that prefix and suffix are strings
        prefixes = process_stringlist(prefixes)
        prefix = ''
        if len(prefixes) != 0:
            if prefixes[-1][-1] != ' ':
                prefix = prefixes.pop()

        suffixes = process_stringlist(suffixes)
        suffix = ''
        if len(suffixes) != 0:
            if suffixes[-1][0] != ' ':
                suffix = suffixes.pop(0)

        for x in list:
            if isinstance(x, SCons.Node.FS.File):
                result.append(x)
                continue
            x = str(x)
            if x:
                result.append(prefixes)
                if prefix:
                    if x[:len(prefix)] != prefix:
                        x = prefix + x
                result.append(x)
                if suffix:
                    if x[-len(suffix):] != suffix:
                        result[-1] = result[-1] + suffix
                result.append(suffixes)
        return result

    env['_concat_list'] = _concat_list
    # Note to self: command 2>&1 | other command appears to work as I would hope
    # except it eats errors
    iwyu = SCons.Builder.Builder(
           action=[
                '$IWYU_MASSAGE $TARGET $IWYU $IWYU_FLAGS $IWYU_MAPPINGS $IWYU_COMCOM $SOURCE'
            ],
            emitter=emitter,
            suffix='.iwyu',
            src_suffix='.cpp')

    env.Append(BUILDERS={'IWYU': iwyu})

    # Sigh - IWYU is a right bum as it doesn't recognise /I so I have to
    # duplicate most of the usual stuff

    env['IWYU_FLAGS'] = [
        # This might turn down the output a bit. I hope
        '-Xiwyu', '--transitive_includes_only',
        # Seem to be needed for a windows build
        '-D_MT', '-D_DLL', '-m32',
        # This is something to do with clang, windows and boost headers
        '-DBOOST_USE_WINDOWS_H',
        # There's a lot of this, disabled for now
        '-Wno-inconsistent-missing-override',
        # Mark boost and Qt headers as system headers to disable a lot of noise.
        # I'm sure there has to be a better way than saying 'prefix=Q'
        '--system-header-prefix=Q',
        '--system-header-prefix=boost/',
        # Should be able to get this info from our setup really
        '-fmsc-version=1800', '-D_MSC_VER=1800',
        # clang and qt don't agree about these because clang says its gcc 4.2
        # and QT doesn't realise it's clang
        '-DQ_COMPILER_INITIALIZER_LISTS',
        '-DQ_COMPILER_DECLTYPE',
        '-DQ_COMPILER_VARIADIC_TEMPLATES',
    ]
    if env['CONFIG'] == 'debug':
        env['IWYU_FLAGS'] += [ '-D_DEBUG' ]

    env['IWYU_DEFPREFIX'] = '-D'
    env['IWYU_DEFSUFFIX'] = ''
    env['IWYU_CPPDEFFLAGS'] = '${_defines(IWYU_DEFPREFIX, CPPDEFINES, IWYU_DEFSUFFIX, __env__)}'

    env['IWYU_INCPREFIX'] = '-I'
    env['IWYU_INCSUFFIX'] = ''
    env['IWYU_CPPINCFLAGS'] = '$( ${_concat(IWYU_INCPREFIX, CPPPATH, IWYU_INCSUFFIX, __env__, RDirs, TARGET, SOURCE)} $)'

    env['IWYU_PCH_PREFIX'] = '-include' # Amazingly this works without a space
    env['IWYU_PCH_SUFFIX'] = ''
    env['IWYU_PCHFILES'] = '$( ${_concat(IWYU_PCH_PREFIX, PCHSTOP, IWYU_PCH_SUFFIX, __env__, target=TARGET, source=SOURCE)} $)'

    env['IWYU_COMCOM'] = '$IWYU_CPPDEFFLAGS $IWYU_CPPINCFLAGS $IWYU_PCHFILES $CCPDBFLAGS'
    env['IWYU_MAPPING_PREFIX'] = ['-Xiwyu', '--mapping_file=']
    env['IWYU_MAPPING_SUFFIX'] = ''
    env['IWYU_MAPPINGS'] = '$( ${_concat_list(IWYU_MAPPING_PREFIX, IWYU_MAPPING_FILE, IWYU_MAPPING_SUFFIX, __env__, f=lambda l: [ str(x) for x in l], target=TARGET, source=SOURCE)} $)'

    env['IWYU_MAPPING_FILE'] = [
        env.File('#/modorganizer/qt5_4.imp'),
        env.File('#/modorganizer/win.imp'),
        env.File('#/modorganizer/mappings.imp')
        ]

    env['IWYU_MASSAGE'] = env.File('#/modorganizer/massage_messages.py')

# Create base environment
vars = setup_config_variables()
env = Environment(variables = vars, TARGET_ARCH = 'x86')

# I'd really like to validate for unexpected settings in 'variables', but scons
# appears to throw them away
#ok = True
#for key, value in vars.UnknownVariables():
#    print "unknown variable in scons_configure.py:  %s=%s" % (key, value)
#    ok = False
#if not ok:
#    sys.exit(1)

# Patch scons to realise it's generating PDB files if /DEBUG is set
env.AppendUnique(PROGEMITTER = [ link_emitter_wrapper ])
# Ditto + windows_embed_manifest doesn't generate a manifest file
env.AppendUnique(SHLIBEMITTER = [ shlib_emitter_wrapper ])

# Work out where to find boost libraries
libdir = os.path.join(env['BOOSTPATH'], 'lib32-msvc-' + env['MSVC_VERSION'])
if not os.path.exists(libdir):
    libdir = os.path.join(env['BOOSTPATH'], 'stage', 'lib')
env.AppendUnique(LIBPATH = libdir)

# Process command line to find out what/where we're building
config = ARGUMENTS.get('CONFIG', 'debug')
env['CONFIG'] = config

# I think this is a bug in scons. It returns 12.0 for studio 13
# This needs to match the QT specified version or bad things will happen!
msvs_version = int(env['MSVS_VERSION'].split('.')[0]) + 2000
if msvs_version > 2010:
    msvs_version += 1

# Need more work on the qt version stuff
#build-ModOrganizer-Desktop_Qt_5_4_1_MSVC2013_OpenGL_32bit-Debug
build_dir = 'scons-ModOrganizer-QT_%s_for_MSVS%d_32bit-%s' % (
    #os.path.basename(env['QTDIR']).replace('.', '_'),
    '5_4_1_OpenGL',
    msvs_version,
    config.title())

# Put the sconsign file somewhere sane
env.SConsignFile(os.path.join(build_dir, '.sconsign.dblite'))

#this doesn't seem to work
#env.VariantDir('build/$CONFIG', 'source')
#env.VariantDir('build/$CONFIG', 'source', duplicate = 0)

# Ripped off from qmake.conf

# Compiler defines. Note that scons uses the same variables for C and C++
# defines and include paths so be careful if you mix languages.

# A note: QT puts _MSC_VER into the compile line, but I can see no earthly
# reason for this.

env.AppendUnique(CPPDEFINES = [
    'UNICODE',
    'WIN32',
    'NOMINMAX' # Nukes boost all over the place
])

# Default warning level.
env['WARNING_LEVEL'] = 3

# C compiler flags
env.AppendUnique(CPPFLAGS = [
    '/Zm200',
    #'/Zc:wchar_t-', # 4 v 5
    '/Zc:wchar_t',
    '/W$WARNING_LEVEL'
])

# C++ compiler flags
env.AppendUnique(CXXFLAGS = [
    '/w34100',
    '/w34189',
    '/EHsc',    # STL on
    '/GR'       # RTTI on
])

"""
# qmake.conf
#QMAKE_CFLAGS_RELEASE    = -O2 -MD
#QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += -O2 -MD -Zi
#QMAKE_CFLAGS_LTCG       = -GL
#QMAKE_CFLAGS_MP         = -MP

QMAKE_LFLAGS_RELEASE    = /INCREMENTAL:NO
QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO = /DEBUG /OPT:REF
QMAKE_LFLAGS_LTCG       = /LTCG

# project
"""

env['WINDOWS_EMBED_MANIFEST'] = True


# Seriously, the linker doesn't apply these by default?
env.AppendUnique(LINKFLAGS = [
    '/DYNAMICBASE',
    '/NXCOMPAT'
])

# Display full path in messages. Sadly even this isn't good enough for Creator
# env.AppendUnique(CPPFLAGS = [ '/FC' ])

if env['CONFIG'] == 'debug':
   env.AppendUnique(CPPFLAGS = [ '/MDd', '/Z7' ])
   env.AppendUnique(LINKFLAGS = [ '/DEBUG' ]) #, '/OPT:REF'])
else:
    env.AppendUnique(CPPFLAGS = [ '/O2', '/MD' ])
    env.AppendUnique(LINKFLAGS = [ '/OPT:REF', '/OPT:ICF' ])

# Set up include what you use. Add this as an extra compile step. Note it
# doesn't currently generate an output file (use the output instead!).
if 'IWYU' in env:
    setup_IWYU(env)

# /OPT:REF removes unreferenced code
# for release, use /OPT:ICF (comdat folding: coalesce identical blocks of code)

# We have to make the install path absolute. *sigh*. But it appears to have to
# be in build_dir...
env['INSTALL_PATH'] = os.path.join(os.getcwd(), build_dir, '_ModOrganizer')

# Create the environment for QT
qt_env = env.Clone()

# If you don't do this for the release build, QT gets very upset...
if qt_env['CONFIG'] != 'debug':
    qt_env.AppendUnique(CPPDEFINES = [ 'QT_NO_DEBUG' ])

# Better way of working this out
qt_env['QT_MAJOR_VERSION'] = 5 #int(os.path.basename(env['QTDIR']).split('.')[0])

qt_env.Tool('qt%d' % qt_env['QT_MAJOR_VERSION'])

# FIXME See if I can work out how to get official scons qt to work. Appears
# to only work with QT5
fixup_qt5()

qt_env.AddMethod(add_moc_files, 'AddExtraMoc')

# A very strange rune which QT sets
qt_env['EXE_MANIFEST_DEPENDENCY'] =\
    '/MANIFESTDEPENDENCY:"' + ' '.join(("type='win32'",
                                        "name='Microsoft.Windows.Common-Controls'",
                                        "version='6.0.0.0'",
                                        "publicKeyToken='6595b64144ccf1df'",
                                        "language='*'",
                                        "processorArchitecture='*'")) + '"'

# Not sure how necessary this is. Moreover it says msvc but seems to expect
# the msvs number
qt_env.AppendUnique(CPPPATH = [
    os.path.join(env['QTDIR'], 'mkspecs', 'win32-msvc%d' % msvs_version)
])

qt_env.AddMethod(EnableQtModules)
qt_env.AddMethod(DisableQtModules)

# This is a hack. we should redirect uic to uic4/uic5 and fix the scripts
def Uicc5(self, *args, **kwargs):
    return self.Uic5(*args, **kwargs)

qt_env.AddMethod(Uicc5, 'Uic')

# Enable the base libraries.
qt_env.EnableQt5Modules([], debug = qt_env['CONFIG'] == 'debug')

# Causes too many problems if you don't do this
qt_env['QT%d_MOCCPPPATH' % qt_env['QT_MAJOR_VERSION']] = '$CPPPATH'
# Yechhh. Note: the the _VER depends on ms compiler version (and could
# be dragged from the qmake conf file or preferrably from ms compiler)
qt_env['QT%d_MOCDEFINES' % qt_env['QT_MAJOR_VERSION']] =\
    '${_defines(QT5_MOCDEFPREFIX, MOCDEFINES+CPPDEFINES, QT5_MOCDEFSUFFIX, __env__)}'
qt_env['MOCDEFINES'] = [
    '_MSC_VER=1800',
]
# QTCreator appears to add these automatically. Some of these look moderately
# dangerous if you're attempting to cross-compile
# only for qt4?
if qt_env['QT_MAJOR_VERSION'] <= 4:
    qt_env.AppendUnique(CPPDEFINES = [
        'QT_DLL',
        'QT_HAVE_MMX',
        'QT_HAVE_3DNOW',
        'QT_HAVE_SSE',
        'QT_HAVE_MMXEXT',
        'QT_HAVE_SSE2',
        'QT_THREAD_SUPPORT'
    ])

# It also adds this to the end of the include path.
# -I"c:\Apps\Qt\4.8.6\include\ActiveQt"

# Export environment. Rename it first to encourage instant Clone() calls
Export('env qt_env')

# And away we go
libs_to_install = env.SConscript('source/SConscript',
                                 variant_dir = build_dir,
                                 duplicate = 0)
libs_to_install = sorted(set(filter(lambda x: x is not None,
                                                 env.Flatten(libs_to_install))))
# There are some odd implicit dependencies
if qt_env['QT_MAJOR_VERSION'] > 4:
    libs_to_install += [
        'Multimedia',
        'MultimediaWidgets',
        'OpenGL',
        'Positioning',
        'PrintSupport',
        'Quick',
        'Sensors',
        'WebChannel',
    ]

# Finally, set up rules to install the DLLs.

dll_path = os.path.join('$INSTALL_PATH', 'DLLs')

prefix, suffix, suffix_ = get_qt_lib_info(qt_env)

dlls_to_install = []
dlls_to_install = [
    os.path.join(env['QTDIR'], 'bin', prefix + lib + suffix + '.dll')
                                                      for lib in libs_to_install
]

if env['CONFIG'] == 'debug':
    dlls_to_install += [
        os.path.join(env['QTDIR'], 'bin', prefix + lib + suffix + '.pdb')
                                                      for lib in libs_to_install
    ]

# There is something wrong with webkit4 and/or this build as it seems
# to need to live in the same directory as mod organiser.
if 'WebKit' in libs_to_install and qt_env['QT_MAJOR_VERSION'] == 4:
    libname = prefix + 'WebKit' + suffix
    env.Install(env['INSTALL_PATH'],
                os.path.join(env['QTDIR'], 'bin', libname + '.dll'))
    if env['CONFIG'] == 'debug':
        env.Install(env['INSTALL_PATH'],
                    os.path.join(env['QTDIR'], 'bin', libname + '.pdb'))

if qt_env['QT_MAJOR_VERSION'] > 4:
    # Guesswork a bit.
    dlls_to_install += [
        os.path.join(env['QTDIR'], 'bin', 'icu%s53.dll' % lib)
                                                    for lib in ('dt','in', 'uc')
    ]

    platform_dlls = []
    if env['CONFIG'] == 'debug':
        platform_dlls += [ 'qwindowsd.dll', 'qwindowsd.pdb' ]
    else:
        platform_dlls += [ 'qwindows.dll' ]

    # Note: Appears to work fine in DLLs or at the top level, but I'm all for
    # keeping the top directory a bit clean
    env.Install(os.path.join(dll_path, 'platforms'),
                [ os.path.join(env['QTDIR'], 'plugins', 'platforms', dll)
                                                     for dll in platform_dlls ])

    image_dlls = []
    for image in ('dds', 'gif', 'jpeg', 'tga'):
        if env['CONFIG'] == 'debug':
            image_dlls += [ 'q' + image + 'd.dll', 'q' + image + 'd.pdb' ]
        else:
            image_dlls += [ 'q' + image + '.dll' ]
    env.Install(os.path.join(dll_path, 'imageformats'),
                [ os.path.join(env['QTDIR'], 'plugins', 'imageformats', dll)
                                                        for dll in image_dlls ])

# Build your own?
dlls_to_install += [
    os.path.join('tools', 'static_data', 'dlls', '7z.dll')
]

env.Install(dll_path, dlls_to_install)

# And loot which goes somewhere else (maybe this should be done in loot_cli?)
env.Install(os.path.join('${INSTALL_PATH}', 'loot'),
            os.path.join('${LOOTPATH}', 'loot32.dll'))

# also pythondll and zip, boost python dll (all for python proxy!)
# the dll we can drag from the python install
# the zip i'm not sure about.
