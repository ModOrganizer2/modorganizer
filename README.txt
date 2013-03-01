HOW TO BUILD
============

Requirements:
-------------

Visual C++ compiler 2010 (VC 10.0) or up
- Included in visual c++ express 2010 or windows sdk 7.0

QtSDK 4.8.x (http://qt-project.org/downloads)
- Qt5 is not yet supported but WIP
- Install according to instruction

boost libraries (http://www.boost.org/)
- Compile according to their instructions (using vc++)
- As of time of writing boost_thread is the only compiled boost library used, you can
  disable the others to safe yourself the compile time

zlib (http://www.zlib.net/)
- Compile static library according to their instructions
- You should have a "zlibstatic.lib" under "build" afterwards

7z.dll (http://www.7-zip.org/download.html)
- Part of the 7-zip program
- Has to be the 32-bit dll! (approx. 893kb)

Recommended:
------------

Qt Creator
- Included in QtSDK but there might be a newer version as a separate download

Set up (using Qt Creator):
--------------------------

1. Using Qt Creator open source/ModOrganizer.pro from your working copy
2. Open the "Projects" tab, open the "Details" for "Build Environment"
3. Click "Add" to add a variable called "BOOSTPATH" with the path of your boost installation as the value (i.e. C:\code\boost_1_52_0)
4. Click "Add" to add a variable called "ZLIBPATH" with the path of your zlib installation as the value (i.e. C:\code\zlib-1.2.7)
5. Switch the build configuration at the very top of the same page from "debug" to "release" (or vice versa) and repeat steps 3 and 4
6. Compile the configuration(s) you want to use (debug and/or release) (Build All). This should compile correctly.
7. return to the "projects" tab and switch to "Run Settings"
For Release build:
8r. Add a "Run configuration" that points to <your working copy>\output\ModOrganizer.exe
9r. Copy "7z.dll" to <your working copy>\output\dlls
10r. From QtSDK\Desktop\Qt\4.8.0\msvc2010\bin copy the following files to <your working copy>\output\dlls: QtCore4.dll, QtDeclarative4.dll, QtGui4.dll, QtNetwork4.dll, QtScript4.dll, QtSql4.dll, QtWebkit4.dll, QtXml4.dll, QtXmlPatterns4.dll
For Debug build:
8d. Add a "Run configuration" that points to <your working copy>\outputd\ModOrganizer.exe
9d. Copy "7z.dll" to <your working copy>\outputd\dlls
10d. From QtSDK\Desktop\Qt\4.8.0\msvc2010\bin copy the following files to <your working copy>\outputd\dlls: QtCored4.dll, QtDeclaratived4.dll, QtGuid4.dll, QtNetworkd4.dll, QtScriptd4.dll, QtSqld4.dll, QtWebkitd4.dll, QtXmld4.dll, QtXmlPatternsd4.dll

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