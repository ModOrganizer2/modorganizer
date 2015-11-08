# Attempt to massage the messages from clang so that QT recognises them
# expected: source\bsatk\bsafile.cpp(101) : fatal error C1189: #error :  "we may have to compress/decompress!"
# clang:    source\bsatk\bsafile.cpp(101) :  error: "we may have to compress/decompress!"

import fileinput
import re
import subprocess
import sys

removing = None

includes = dict

foundline = 0

def process_next_line(line):
    # Look for '\) : ([^:])?:'
    # Replace with ') : \1 I0000:
    global removing
    global includes
    global foundline
    line = line.rstrip()
    if removing:
        if line == '':
            removing = None
            print
            return
        else:
            # Really we should stash these so that if we get a 'class xxx' in the
            # add lines we can print it here. also we could do the case fixing.
            m = re.match(r'- #include [<"](.*)[">] +// lines (.*)-', line)
            if m:
                # If there is an added line with the same class, print it here
                print '%s(%s) : warning I0001: Unnecessary include of %s' % (removing, m.group(2), m.group(1))
                foundline = m.group(1)
            else:
                m = re.match(r'- (.*) +// lines (.*)-', line)
                if m:
                    print '%s(%s) : warning I0002: Unnecessary forward ref of %s' % (removing, m.group(2), m.group(1))
                    foundline = m.group(1)
                else:
                    print '********* I got confused **********'

    # Replace clang :line:column: type: with ms (line) : type nnnn:
    # filename:line:column type:
    # replace with brackets
    line = re.sub(r':(\d+):\d+: ([^:]*):', r'(\1) : \2 I1234:', line)

    if ': note I1234:' in line:
        line = '        ' + line
        line = line.replace(': note I1234:', '')

    print line
    if line.endswith(' should remove these lines:'):
        removing = (line.split(' '))[0]
    elif line.endswith(' should add these lines:'):
        adding = (line.split(' '))[0]

# also process the other lines
"""

source/organizer/aboutdialog.h should add these lines:
#include <QObject>  // for Q_OBJECT, slots
#include <QString>  // for QString
class QListWidgetItem;
class QWidget;

source/organizer/aboutdialog.h should remove these lines:
- #include <QListWidgetItem>  // lines 25-25
- #include <utility>  // lines 28-28
- #include <vector>  // lines 27-27
- class DownloadManager;  // lines 47-47

The full include-list for source/organizer/aboutdialog.h:
#include <QDialog>  // for QDialog
#include <QObject>  // for Q_OBJECT, slots
#include <QString>  // for QString
#include <map>      // for map
class QListWidgetItem;
class QWidget;
namespace Ui { class AboutDialog; }  // lines 31-31
---
"""

    # added lines should come after the first entry with a line number.

process = subprocess.Popen(sys.argv[1:],
                           stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
while True:
    output = process.stdout.readline()
    if output == '' and process.poll() is not None:
        break
    if output:
        process_next_line(output)
rc = process.poll()
# The return code you get appears to be more to do with the amount of output
# generated than any real error. We should error if any ': error:' lines are
# detected

