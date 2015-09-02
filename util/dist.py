# -*- Mode: Python -*-

import os
import time

execfile ('clean.py')
os.chdir ('..')
tarball = 'coro++.%s.tar.gz' % (time.strftime ('%y%m%d'),)
# COPYFILE_DISABLE is a hack to turn off storing resource forks in OSX.
os.system ('COPYFILE_DISABLE=true tar --exclude .git -zcvf %s coro++' % (tarball,))

# test bootstrap to get official self/compile.c
os.system ('mkdir coro++-staging')
os.chdir ('coro++-staging')
os.system ('tar -zxvf ../%s' % (tarball,))
os.chdir ('coro++')
execfile ('clean.py')
os.chdir ('..')
# remake the tarball
os.system ('COPYFILE_DISABLE=true tar --exclude .svn -zcvf %s coro++' % (tarball,))

# clean up
os.system ('rm -fr coro++-staging')
os.chdir ('coro++')
