from collections import defaultdict

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
includes = dict()

adding = None
added = dict()
lcadded = dict()

foundline = None

errors = False

messages = defaultdict(list)

def process_next_line(line, outfile):
    """ Read a line of output/error from include-what-you use
        Turn clang errors into a form QT creator recognises
        Raise warnings for unneeded includes
    """
    global removing
    global includes
    global foundline
    global errors
    global added
    global adding
    line = line.rstrip()
    print >> outfile, line

    if line.endswith(' should remove these lines:'):
        adding = None
        removing = (line.split(' '))[0]
    elif line.endswith(' should add these lines:'):
        removing = None
        adding = (line.split(' '))[0]
    elif line == '' or line.startswith(' the full include-list'):
        adding = None
        removing = None
    elif adding:
        m = re.match(r'.*class (.*);', line)
        if m:
            added[m.group(1)] = (adding, line)
            lcadded[m.group(1).lower() + '.h'] = m.group(1)
        else:
            added[line] = (adding, line)
    elif removing:
        # Really we should stash these so that if we get a 'class xxx' in
        # the add lines we can print it here. also we could do the case
        # fixing.
        m = re.match(r'- #include [<"](.*)[">] +// lines (.*)-', line)
        if m:
            foundline = m.group(2)
            # Note: In this project at least we have a naming convention of
            # lower case filename and upper case classname.
            clname = m.group(1)
            if clname not in added:
                if clname in lcadded:
                    clname = lcadded[clname]
            if clname in added:
                messages[removing].append(
                    '%s(%s) : warning I0004: Replace include of %s with '
                        'forward reference %s' % (
                        removing, m.group(2), m.group(1), added[clname][1]))
                del added[clname]
            else:
                messages[removing].append(
                    '%s(%s) : warning I0001: Unnecessary include of %s' % (
                                              removing, m.group(2), m.group(1)))

        else:
            m = re.match(r'- (.*) +// lines (.*)-', line)
            if m:
                messages[removing].append(
                    '%s(%s) : warning I0002: Unnecessary forward ref of %s' % (
                                              removing, m.group(2), m.group(1)))
            else:
                print '********* I got confused **********'
    elif line.startswith('In file included from'):
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

outfile = open(sys.argv[1], 'w')
process = subprocess.Popen(sys.argv[2:],
                           stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
while True:
    output = process.stdout.readline()
    if output == '' and process.poll() is not None:
        break
    if output:
        process_next_line(output, outfile)

# A note: We should probably do some work as we use the source code line for
# messages in include files...

if foundline is None:
    foundline = '1'

for add in added:
    messages[added[add][0]].append(
        '%s(%s) : warning I0003: Need to include %s' % (
                                       added[add][0], foundline, added[add][1]))

for file in sorted(messages.keys(), reverse = True):
    for line in messages[file]:
        print line

rc = process.poll()
# The return code you get appears to be more to do with the amount of output
# generated than any real error, so instead we should error if any ': error:'
# lines are detected

if errors:
    sys.exit(1)
