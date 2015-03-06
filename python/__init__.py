"""satyr - python bindings for satyr library"""

import sys

if sys.version_info[0] == 2:
    from ._satyr import *
else:
    from ._satyr3 import *
