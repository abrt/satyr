#!/usr/bin/env python

import os
import unittest

try:
    import _satyr as satyr
except ImportError:
    import satyr


path = '../gdb_stacktraces/rhbz-803600'
threads_expected = 2
frames_expected = 227

if not os.path.isfile(path):
    path = '../' + path

with file(path) as f:
    contents = f.read()

class TestGdbStacktrace(unittest.TestCase):
    def setUp(self):
        self.trace = satyr.GdbStacktrace(contents)

    def frame_count(self, trace):
        return sum(map(lambda x: len(x.frames), trace.threads))

    def test_correct_thread_count(self):
        self.assertEqual(len(self.trace.threads), threads_expected)

    def test_correct_frame_count(self):
        self.assertEqual(self.frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(dup.threads, self.trace.threads)

        dup.threads = dup.threads[:5]
        dup2 = dup.dup()
        self.assertEqual(len(dup.threads), len(dup2.threads))

    def test_prepare_linked_list(self):
        dup = self.trace.dup()
        dup.threads = dup.threads[:5]
        dup.normalize()
        self.assertTrue(len(dup.threads) <= 5)

    def test_normalize(self):
        dup = self.trace.dup()
        dup.normalize()
        self.assertNotEqual(self.frame_count(dup), self.frame_count(self.trace))

    def test_str(self):
        out = str(self.trace)
        self.assertIn('Stacktrace with %d threads' % threads_expected, out)

class TestGdbThread(unittest.TestCase):
    def setUp(self):
        self.thread = satyr.GdbStacktrace(contents).threads[0]

class TestGdbSharedlib(unittest.TestCase):
    def setUp(self):
        self.shlib = satyr.GdbStacktrace(contents).libs[0]

class TestGdbFrame(unittest.TestCase):
    def setUp(self):
        self.frame = satyr.GdbStacktrace(contents).threads[0].frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertIn('0x0000003ec220e48d', out)
        self.assertIn('write', out)
        self.assertIn('Frame #0', out)

    def test_dup(self):
        dup = self.frame.dup()
        self.assertEqual(dup.get_function_name(),
            self.frame.get_function_name())

        dup.set_function_name('other')
        self.assertNotEqual(dup.get_function_name(),
            self.frame.get_function_name())

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertEqual(dup.cmp(dup, 0), 0)
        self.assertEqual(dup.cmp(self.frame, 0), 0)
        self.assertEqual(dup.cmp(self.frame, 0), 0)
        dup.set_function_name('another')
        self.assertNotEqual(dup.cmp(self.frame, 0), 0)

if __name__ == '__main__':
    unittest.main()
