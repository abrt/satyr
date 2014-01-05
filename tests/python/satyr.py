import sys
import os.path

oldpath = sys.path
newpath = os.path.join(os.path.dirname(__file__), '../../python/.libs')
sys.path = [newpath]

if sys.version_info[0] == 2:
    from _satyr import *
else:
    from _satyr3 import *

sys.path = oldpath
del sys, os
del oldpath, newpath
