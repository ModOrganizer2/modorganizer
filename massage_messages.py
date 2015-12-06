import fileinput
import re
import subprocess
import sys


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
removing = None

includes = dict

foundline = 0

errors = False

def process_next_line(line, outfile):
    """ Read a line of output/error from include-what-you use
        Turn clang errors into a form QT creator recognises
        Raise warnings for unneeded includes
    """
    global removing
    global includes
    global foundline
    global errors
    line = line.rstrip()
    print >> outfile, line
    if removing:
        if line == '':
            removing = None
            print
            return
        else:
            # Really we should stash these so that if we get a 'class xxx' in
            # the add lines we can print it here. also we could do the case
            # fixing.
            m = re.match(r'- #include [<"](.*)[">] +// lines (.*)-', line)
            if m:
                # If there is an added line with the same class, print it here
                print '%s(%s) : warning I0001: Unnecessary include of %s' %\
                                              (removing, m.group(2), m.group(1))
                foundline = m.group(1)
            else:
                m = re.match(r'- (.*) +// lines (.*)-', line)
                if m:
                    print '%s(%s) : warning I0002: '\
                                              'Unnecessary forward ref of %s' %\
                                              (removing, m.group(2), m.group(1))
                    foundline = m.group(1)
                else:
                    print '********* I got confused **********'

    if line.startswith('In file included from'):
        line = re.sub(r'^(In file included from)(.*):(\d+):',
                      r'        \2(\3) : \1 here',
                      line)
        # Note; QT Creator seems to be unwilling to let you double click the
        # line to select the code in question if you get a string of these, not
        # sure why.
    elif ': note:' in line:
        line = '        ' + re.sub(r':(\d+):\d+: note:', r'(\1) : note:', line)
    else:
        # Replace clang :line:column: type: with ms (line) : type nnnn:
        line = re.sub(r':(\d+):\d+: ([^:]*):', r'(\1) : \2 I1234:', line)
        if ' : error I1234:' in line:
            errors = True

    print line
    if line.endswith(' should remove these lines:'):
        removing = (line.split(' '))[0]
    elif line.endswith(' should add these lines:'):
        adding = (line.split(' '))[0]

# also process the other lines

    # added lines should come after the first entry with a line number.

outfile = open(sys.argv[1], 'w')
process = subprocess.Popen(sys.argv[2:],
                           stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
while True:
    output = process.stdout.readline()
    if output == '' and process.poll() is not None:
        break
    if output:
        process_next_line(output, outfile)

rc = process.poll()
# The return code you get appears to be more to do with the amount of output
# generated than any real error, so instead we should error if any ': error:'
# lines are detected

if errors:
    sys.exit(1)
