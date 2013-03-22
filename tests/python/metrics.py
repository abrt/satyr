#!/usr/bin/env python

import os
import unittest

try:
    import _satyr as satyr
except ImportError:
    import satyr

class TestDistances(unittest.TestCase):
    def setUp(self):
        pass

    def test_constructor(self):
        """
        When a thread is modified in python (e.g. removed frames), these
        changes are not visible in the c code, the list of frames doesn't seem
        to be rebuilt from the python list.
        See: https://github.com/abrt/satyr/issues/62
        """

        # same threads, distance ought to be 0
        thread1 = satyr.GdbThread("f1 lib1\n", True)
        thread2 = satyr.GdbThread("f1 lib1\n", True)

        distances = satyr.Distances("levenshtein", [thread1, thread2], 2)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.0)

        # original backtrace
        thread1 = satyr.GdbThread("f1 lib1\nf2 lib2\nf3 lib3\n", True)
        thread2 = satyr.GdbThread("f1 lib1\nf4 lib4\nf5 lib5\n", True)

        distances = satyr.Distances("levenshtein", [thread1, thread2], 2)
        self.assertNotAlmostEqual(distances.get_distance(0, 1), 0.0)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.66666668)

        # keep only the first frame, should behave the same as the first two
        # threads
        thread1.frames[1:] = []
        thread2.frames[1:] = []

        distances = satyr.Distances("levenshtein", [thread1, thread2], 2)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.0)

if __name__ == '__main__':
    unittest.main()
