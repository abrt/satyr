import sys
import os.path

oldpath = sys.path
newpath = os.path.join(os.path.dirname(__file__), '../../python/.libs')
sys.path = [newpath]
from _satyr import *

sys.path = oldpath
del sys, os
del oldpath, newpath
