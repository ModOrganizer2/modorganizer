HOW TO BUILD
============

Intro:
------

Building Mod Organizer can be a rather daunting task especially if you're not very comfortable with C++ development under windows.
Please note that if you only want to work on and build a plugin you can save yourself a lot of trouble

Overview:
---------

As of July 2014 MO consists of the following subprojects:
- organizer: The main userinterface. heavy usage of various libraries
- hookdll: core library of the virtual file system
- uibase: interop between plugins and the main application as well as some reusable functionality
- shared: functionality shared between organizer and hookdll. I'm attempting to get rid of this library over time
- nxmhandler: tool to pass handling of nxm links to MO or other applications
- helper: tool for doing operations requiring elevated priviledges if MO doesn't have them
- proxydll: dll used in the "proxy dll" load mechanism
- esptk: small library containing functionality to work with esps/esms
- bsatk: small library containing functionality to work with bsas. Requires zlib to extract files. Requires boost_thread to provide multi-threaded extraction
- archive: small wrapper library around 7zip for dealing with mod archives. Requires 7zip
- NCC: extension to NMM to provide a binary with command line interface for fomod installation. This is c# code and does not build with the rest of the project. Requires the rest of NMM
- bossdummy: dummy dll that looks like the boss.dll. This is used instead of the real boss dlls in NCC to save some disk space
- pythonrunner: library for embedding python code. Requires boost_python, python 2.7 and pyqt5
- loot_cli: this is a command line client of loot. This can be better integrated with MO than the official loot client.

And various plugins:
- bsaExtractor: offers to extract bsas after a mod has been installed
- checkFNIS: Activates each time an application is started from MO and runs fnis if necessary
- diagnoseBasic: Various diagnostic checks on the game
- inieditor: minimalistic file editor for ini files
- installerBain: handles non-scripted bain installers
- installerBundle: handles installation of archives wrapped in archives
- installerFomod: handles installation of xml fomods
- installerManual: handles installations of archives that aren't supported by any other plugin (or if the user chooses to do the installation manually)
- installerNCC: handles installation of any fomod (requires NCC)
- installerQuick: handles very simple one-click installations
- NMMImport: importer from existing nmm installation
- proxyPython: integrates pythonrunner as a plugin into MO
- pyniEdit: more user-friendly ini editor. python code
- previewBase: used for file previews. this plugin covers image formats supported by the qt installation (usually at least jpg, gif, png) and text files
- previewDDS: used to preview dds textures. uses code from the nif file format library

There are a few more plugins that are either broken or samples
- installerBCF: this was intended to use .bcf (bain conversion file) files as installation instructions but currently it is completely function-less
- helloWorldCpp: sample for cpp plugins. This should compile even without fulfilling most dependencies below
- pageTESAlliance: integrates the tes alliance page into MO. This integration is not nearly as tight as that of nexus.

Requirements:
-------------

Visual C++ compiler 2013 (VC 12.0) or up
- Included in visual c++ express 2013
- Do install windows sdk too if only for the cdb debugger which is included in that package
Note: If you're having trouble installing the windows sdk, you may be affected by this bug: http://support.microsoft.com/kb/2717426

Qt Libraries 5.4.x (http://www.qt.io/download-open-source/#section-2)
- i.e. "Qt libraries 5.4.0 for Windows (VS 2013, 722 MB)"
- tested: 5.4.0 for Windows 32-bit (VS 2013, OpenGL, 695 MB)
- Install according to instruction

boost libraries (http://www.boost.org/)
- tested: 1.56
- Build according to their instructions (using vc++): http://www.boost.org/doc/libs/1_57_0/more/getting_started/windows.html
- A few of the boost libraries need to be built (the rest is header-only). The only compiled libs MO needs (at the time of writing) are
  boost_thread (for everything that links agains bsatk) and boost_python (for pythonrunner). You can disable the others to save yourself compile time (even on a modern system compiling boost can easily take an hour)

zlib (http://www.zlib.net/)
- Compile static library according to their instructions
- Depending on the version of zlib and how you built you should have a file called zlibstatic.lib or zlibstat.lib in "build" or "x86\ZLibStatRelease".
- Please copy that lib file to "build\zlibstatic.lib" (if it doesn't exist) so MO finds it.

7z sourcecode (http://www.7-zip.org/download.html)
- only for "archive" subproject
- no need to compile, will be linked into project

Python 2.7 (32-bit)
- only for pythonrunner

PyQt5 for Python 2.7 (http://www.riverbankcomputing.co.uk/software/pyqt/download)
- tested: PyQt-gpl-5.4.zip
- only for pythonrunner

SIP (http://www.riverbankcomputing.com/software/sip/download)
- presumably only for pythonrunner as it's needed for PyQT5

Recommended:
------------

Qt Creator
- included in Qt SDK
- With Qt Creator usually the rule is "the newer the better"
- Start Qt Creator and check if things are set up correctly:
  1) Go to Tools > Options > Build & Run > Qt Versions
	2.1) If the QtSDK 5.4.x you installed earlier is not auto-detected, click "Add" to add it manually
	2.2) Navigate to your <your qt5 installation>\bin\qmake.exe
	2.3) Restart qt creator
	3) Go to Tools > Options > Build & Run > Kits
	3.1) If no Kit is auto-detected (or the auto-detected ones have picked up the wrong qt or vc++ installation) click "Add"
	3.2) Enter a Name (i.e. "Qt 5.4 MSVC2013 32bit")
	3.3) In Compiler select the visual studio compiler installed earlier (may show up as "Microsoft Windows SDK for Windows x). Make sure you select the x86 variant
	3.4) CDB should be auto-detected
	3.5) Select the qt version set up earlier
	4) Apply and close the options

Set up (using Qt Creator):
--------------------------

1. Create a file called "LocalPaths.pri" in your ModOrganizer\source directory
2. Open that file with any text editor and enter (one entry per line)
2a. BOOSTPATH=<path to your boost installation> (i.e. C:\code\boost_1_56_0)
2b. LOOTPATH=C:/Users/Tannin/Documents/Projects/loot_api
2c. PYTHONPATH=D:/Python278
2d. SEVENZIPPATH=D:/Documents/Projects/7zip
2e. ZLIBPATH=D:/Documents/Projects/zlib-1.2.7
3. Using Qt Creator open source/ModOrganizer.pro from your working copy
4. Compile the configuration(s) you want to use (debug and/or release) (Build All). This should compile correctly (though right now it produces a lot of warnings in boost headers).
5. <reserved for future use. Maybe grab a coffee?>
6. return to the "projects" tab and switch to "Run Settings"
7. Determine the folder of the qt binaries in the package you downloaded. This could be "QtSDK\Desktop\Qt\5.4.0\msvc2013\bin" or "Qt5.4.0\bin"
For Release build:
8r. Add a "Run configuration" that points to <your working copy>\output\ModOrganizer.exe
9r. Copy "7z.dll" to <your working copy>\output\dlls
10r. From the qt binaries directory, copy the following files to <your working copy>\output\dlls: Qt5Core.dll, Qt5Declarative.dll, Qt5Gui.dll, Qt5Network.dll, Qt5Script.dll, Qt5Sql.dll, Qt5Webkit.dll, Qt5Xml.dll, Qt5XmlPatterns.dll
For Debug build:
8d. Add a "Run configuration" that points to <your working copy>\outputd\ModOrganizer.exe
9d. Copy "7z.dll" to <your working copy>\outputd\dlls
10d. From the qt binaries copy the following files to <your working copy>\outputd\dlls: Qt5Cored.dll, Qt5Declaratived.dll, Qt5Guid.dll, Qt5Networkd.dll, Qt5Scriptd.dll, Qt5Sqld.dll, Qt5Webkitd.dll, Qt5Xmld.dll, Qt5XmlPatternsd.dll

Now you should be able to compile and run Mod Organizer.
Please note that when you change anything apart from the "organizer" subproject, qt creator may not pick up on the changes
and not recompile the modified subproject. the "organizer" project on the other hand is always re-linked. If anyone knowledgeable enough with qmake
can fix that that would be awesome.

Set up (without Qt Creator):
----------------------------

1. open a command line shell and navigate to <your working copy>\source
2. run "qmake -tp vc" to create a visual c++ solution
3. Open that solution
4. - 98. A miracle happens
99. You can now compile MO using VC

I'm not using this workflow so I can't give more detailed instructions.
Please note that the primary project format of MO remains qmake, if you work with visual studio and make changes to the project (like adding
source files) you have to manually transfer those changes to the .pro file.



-----------------
Troubleshooting (thanks to Ross):
-----------------

1) When I try to build, I am seeing the error "error: LNK1104: cannot open file 'zlibstatic.lib'"

	a) Make sure you have the following files
		{ZLIBPATH}\build\zconf.h
  		{ZLIBPATH}\build\zlib.h
  		{ZLIBPATH}\build\zlibstatic.lib

	b) make sure "zlibstat.lib" was renamed to "zlibstatic.lib"
	c) currently, there are references to zlibstatic in the following 2 project files:
		./bsatk/bsatk.pro
		./hookdll/hookdll.pro

	Please note if building this, you need to switch OFF -DZLIB_WINAPI ...

2) After building, I try to open {MOPROJECTPATH}\outputd\ModOragnizer.exe and get this error
	"The program can't start because QtDeclaratived4.dll is missing from your computer. Try reinstalling the program to fix this problem."

		-> When setting up Qt4.8.x, make sure you copied the "d" (debug) versions of the dll files to {MOPROJECTPATH}\outputd\dlls
		   If you copied the release version or did not copy the dlls at all, you may see this error.
		   The *.dll files can be found in {QT48xPATH}\bin

		Similiar:
		http://stackoverflow.com/questions/11196416/cant-start-qt-exe-file-on-windows-7

3) I am getting one or more of the the following compiler warnings...

	"{MOPROJECTPATH}\source\bsatk\bsaexception.cpp:32: warning: C4996: 'vsnprintf': This function or variable may be unsafe.
	Consider using vsnprintf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details."
		-> Ignore/suppress warning like this and make be ultra-careful using printf-style functions. vsnprintf_s is a visual-c++ only function so using those would make porting (i.e. to mingw) harder
	
	"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\INCLUDE\stdio.h:354: see declaration of 'vsnprintf'"
		-> see previous
	
	"debug\bsaarchive.obj:-1: warning: LNK4042: object specified more than once; extras ignored"

		-> This appears to be an issue when using VC++ 2010 as the compiler. There are some posts on StackOverflow
			which suggest fixes when using VC++ as the IDE, but not sure how to apply them to Qt Creator.
				http://stackoverflow.com/questions/3729515
				http://stackoverflow.com/questions/3695174
			
		-> Also, not sure if this affects VC++ 2012 / VC++ 2013 / GCC via cygwin/mingw.
			Not even sure if the project would successfully compile on these.
	
	"{MOPROJECTPATH}\source\organizer\mainwindow.cpp:3995: warning: C4428: universal-character-name encountered in source"
	
		-> very pointless warning.
			http://stackoverflow.com/questions/11589571
			http://stackoverflow.com/questions/7078013
			
			the best solution I saw was to just disable the warning altogether...
			#pragma warning( disable : 4428 )

	"warning: LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library"
	
	-> You can always add a #pragma to ignore them :P
		http://msdn.microsoft.com/en-us/library/2c8f766e%28v=vs.80%29.aspx
	
4) I got the following error while building...
	"error: LNK1201: error writing to program database
	'{MOPROJECTPATH}\{whatever-your-build-directory-name-is}\organizer\debug\ModOrganizer.pdb';
	check for insufficient disk space, invalid path, or insufficient privilege"
	
	-> I had this happen after trying to look at something in memory via Process Explorer.
	   To resolve, I killed and restarted PE. Alternatively, you could probably use the
	   Unlocker utility. After unlocking, I manually deleted everything in the directory.

	   Unlocker: http://www.filehippo.com/download_unlocker/ or http://www.emptyloop.com/unlocker/
	   Warning: With Unlocker v1.9.2, you must select "Advanced" then uncheck everything to
	   avoid having some Delta toolbar garbage installed...

5) Problem TT has seen:
    Complains about 'qtwebkit4' missing. For some reason this seems to insist on
    living with modorganiser.exe

6) Debugging the python proxy: I (TT) don't think this is possible unless you have a debug
   build of PyQt. However, a debug build of PyQt requires a debug build of python. There doesn't
   appear to be a half way house which is build wiht normal python but debug versions of QT libraries.
   You can confuse pyqt by copying the debug QT DLLs to appropriate named files in the DLLs
   directory, and removing QT from your project run path, which results in the python proxy
   initialising succesfully, but causes mod organiser to crash in strange places.

-------------------
Building with scons
-------------------

1) Download scons from www.scons.org and install
2) Download QT4 (and/or QT5) from https://bitbucket.org/dirkbaechle/scons_qt4 and 
    https://bitbucket.org/dirkbaechle/scons_qt5. Install as per instructions
3) Copy scons_configure_template.py to scons_configure.py. Edit to point to your
   boost/python/zlib/7zip/loot directories as appropriate.
4) Create build kits as follows:
       Custom Process step:
          Command: <python path>\Scripts\scons.bat
          Arguments: -u .
          Working directory: %{sourcedir}

       Clean Custom Process step:
          Command: <python path>\Scripts\scons.bat
          Arguments: -c -u .
          Working directory: %{sourcedir}

    For the release build, add 'CONFIG=release' to the arguments.
    
    If you want to do parallel builds, add -j and a number to the arguments.
    However you may need to install pywin32 (but it seems to work OK for me).
    -j 4 did the entire build in 2m 15s. scons uses -Z7 to store debugging
    information in the .obj files which removes the issues with multiple
    processes attempting to access one datafile.

    Note. If you want to build a sub project separately, it seems to be better
    to open the subproject, set that up with the scons kit and build that as a
    project.
    
    Please note: the generated (runnable) output files end up in the build directory,
    in the '_ModOrganizer' subdirectory (not at the top level). Named like that
    so you can see it! The scons build will also populate the directory with
    all the necessary DLLs (but see note 6 above about your build tool path if
    trying to debug).
