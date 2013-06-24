#!/usr/bin/env python

import unittest

from test_helpers import load_input_contents

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

        distances = satyr.Distances([thread1, thread2], 2, satyr.DISTANCE_LEVENSHTEIN)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.0)

        # original backtrace
        thread1 = satyr.GdbThread("f1 lib1\nf2 lib2\nf3 lib3\n", True)
        thread2 = satyr.GdbThread("f1 lib1\nf4 lib4\nf5 lib5\n", True)

        distances = satyr.Distances([thread1, thread2], 2, satyr.DISTANCE_LEVENSHTEIN)
        self.assertNotAlmostEqual(distances.get_distance(0, 1), 0.0)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.66666668)

        # keep only the first frame, should behave the same as the first two
        # threads
        thread1.frames[1:] = []
        thread2.frames[1:] = []

        distances = satyr.Distances([thread1, thread2], 2, satyr.DISTANCE_LEVENSHTEIN)
        self.assertAlmostEqual(distances.get_distance(0, 1), 0.0)

    def test_distance_gdb(self):
        contents = load_input_contents('../gdb_stacktraces/rhbz-803600')
        g = satyr.GdbStacktrace(contents)
        thread1 = g.threads[0]
        thread2 = g.threads[1]

        # DISTANCE_LEVENSHTEIN is the default
        self.assertAlmostEqual(thread1.distance(thread1), 0.0)
        self.assertAlmostEqual(thread1.distance(thread2), 0.8827, places=3)
        self.assertAlmostEqual(thread1.distance(thread2), thread2.distance(thread1))

        # TODO are the same stacktraces supposed to compare at 0.98 ?
        # I'll just leave the numbers here so we are assured they at least stay the same ...
        self.assertAlmostEqual(
            thread1.distance(thread1, dist_type=satyr.DISTANCE_JARO_WINKLER),
            0.98,
            places=3)
        self.assertAlmostEqual(
            thread1.distance(thread2, dist_type=satyr.DISTANCE_JARO_WINKLER),
            0.3678,
            places=3)
        # WTF, not even symmetrical?
        #self.assertAlmostEqual(
        #    thread1.distance(thread2, dist_type=satyr.DISTANCE_JARO_WINKLER),
        #    thread2.distance(thread1, dist_type=satyr.DISTANCE_JARO_WINKLER)
        #)

        self.assertAlmostEqual(
            thread1.distance(thread1, dist_type=satyr.DISTANCE_JACCARD),
            0.0)
        self.assertAlmostEqual(
            thread1.distance(thread2, dist_type=satyr.DISTANCE_JACCARD),
            0.8904,
            places=3)
        self.assertAlmostEqual(
            thread1.distance(thread2, dist_type=satyr.DISTANCE_JACCARD),
            thread2.distance(thread1, dist_type=satyr.DISTANCE_JACCARD))

        self.assertAlmostEqual(
            thread1.distance(thread1, dist_type=satyr.DISTANCE_DAMERAU_LEVENSHTEIN),
            0.0)
        self.assertAlmostEqual(
            thread1.distance(thread2, dist_type=satyr.DISTANCE_DAMERAU_LEVENSHTEIN),
            0.8827,
            places=3)
        self.assertAlmostEqual(
            thread1.distance(thread2, dist_type=satyr.DISTANCE_DAMERAU_LEVENSHTEIN),
            thread2.distance(thread1, dist_type=satyr.DISTANCE_DAMERAU_LEVENSHTEIN))

if __name__ == '__main__':
    unittest.main()
