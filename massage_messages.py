# Attempt to massage the messages from clang so that QT recognises them
# expected: source\bsatk\bsafile.cpp(101) : fatal error C1189: #error :  "we may have to compress/decompress!"
# clang:    source\bsatk\bsafile.cpp(101) :  error: "we may have to compress/decompress!"

import fileinput
import re

for line in fileinput.input():
    # Look for '\) : ([^:])?:'
    # Replace with ') : \1 I0000:
    line.rstrip()
    print re.sub(r'\)\s*:\s*([^:]*):', r') : \1 I0000:', line)