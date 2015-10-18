# This python script contains the configuration for scons
# Copy this to scons_configure.py and adjust to taste.

# Path to your boost install - it should have a boost/ subdirectory and a stage/
# subdirectory. The scons script will use stage/lib if there, or the appropriate
# version for your compiler, if you installed the multiple-build version
BOOSTPATH = r"C:\Apps\boost_1_55_0"

# Version of Visual Studio to use, if you wish to use a specific version. If you
# don't specify a version, the latest will be picked.. See the scons manual for
# supported values.
#MSVC_VERSION = '10.0Exp'

# Path to your python install
# You don't really need to set this up but you might if (say) you have a 32- and
# 64-bit python install and scons has been installed for the 64 bit version
#PYTHONPATH=r"C:\Apps\Python"

# Path to your QT install. This might constrain the version of MSVC you can use.
# This seems to be set by QTCreator
#QTDIR = r"C:\Apps\Qt\4.8.6"

# Path to 7-zip sources
SEVENZIPPATH = r"C:\Apps\7-Zip\7z920"

# Path to zlib. Please read the README file for more information about how this
# needs to be set up
ZLIBPATH = r"C:\Apps\zlib-1.2.8"

# Source control programs. Sadly I can't get this information from qt, even
# though you have to set it up in the configuration
GIT = r"C:\Program Files\git\bin\git.exe"
MERCURIAL = r"C:\Program Files\TortoiseHg\hg.exe"
