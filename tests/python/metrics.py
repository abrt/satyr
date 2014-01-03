#!/usr/bin/env python

import unittest
from test_helpers import *

class TestDistances(unittest.TestCase):
    def setUp(self):
        self.threads = [
            satyr.GdbThread("f1 lib1\nf2 lib2\nf3 lib3\n", True),
            satyr.GdbThread("f1 lib1\nf4 lib4\nf5 lib5\n", True),
            satyr.GdbThread("f4 lib4\nf4 lib4\nf3 lib3\n", True),
            satyr.GdbThread("f9 lib9\n", True),
            satyr.GdbThread("f1 lib1\nf2 lib2\nf4 lib4\nf5 lib5\n", True),
            satyr.GdbThread("f1 lib1\nf4 lib4\nf4 lib4\nf9 lib9\ng libg\n", True)
        ]

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

    def assert_correct_matrix(self, dist_from_parts, threads):
        dist_reference = satyr.Distances(threads, len(threads))

        (m, n) = dist_reference.get_size()
        self.assertEqual(dist_from_parts.get_size(), (m, n))
        for i in range(m):
            for j in range(n):
                self.assertEqual(dist_from_parts.get_distance(i, j),
                                 dist_reference.get_distance(i, j))

    def test_distances_part_sequential(self):
        def do_test(threads, nparts):
            parts = satyr.DistancesPart.create(len(threads), nparts)
            for p in parts:
                p.compute(threads)

            dist_from_parts = satyr.Distances.merge_parts(parts)
            self.assert_correct_matrix(dist_from_parts, threads)

        do_test(self.threads[:2], 1)
        do_test(self.threads[:2], 2)
        do_test(self.threads[:4], 1)
        do_test(self.threads[:4], 2)
        do_test(self.threads[:4], 3)
        do_test(self.threads[:4], 4)
        do_test(self.threads[:4], 42)
        for n in [1, 2, 3, 4, 5, 6, 7, 8, 16, 32, 9000]:
            do_test(self.threads, n)

if __name__ == '__main__':
    unittest.main()
