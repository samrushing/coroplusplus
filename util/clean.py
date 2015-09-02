# -*- Mode: Python -*-

import os
import sys

def clean_c (path, precious):
    for root, dirs, files in os.walk (path, topdown=False):
        if root.find ('.dSYM') != -1:
            # get rid of annoying MacOS .dSYM directories
            for name in files:
                os.remove (os.path.join (root, name))
            os.rmdir (root)
        else:
            for name in files:
                jp = os.path.join (root, name)
                if name.endswith ('.o') or (name.endswith ('.c') and name not in precious):
                    os.remove (jp)
                else:
                    stat = os.stat (jp)
                    if stat.st_mode & 1:
                        # an executable
                        os.remove (jp)

for path in ('. test'.split()):
    clean_c (path, [])

def unlink (p):
    try:
        os.unlink (p)
    except:
        pass
