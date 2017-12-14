#!/usr/bin/python

#
# this file exists for the sole reason of _satyr module appearing as satyr
# without the underscore in the generated documentation
#

import os, sys

sys.path.insert(0, os.path.abspath('../.libs'))

if sys.version_info[0] == 2:
    from _satyr import *
else:
    from _satyr3 import *
