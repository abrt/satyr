#!/usr/bin/env python

import os
import unittest

try:
    import _satyr as satyr
except ImportError:
    import satyr

path = '../java_stacktraces/java-02'
threads_expected = 1
frames_expected = 32

if not os.path.isfile(path):
    path = '../' + path

with file(path) as f:
    contents = f.read()

class TestJavaStacktrace(unittest.TestCase):
    def setUp(self):
        self.trace = satyr.JavaStacktrace(contents)

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
        dup2 = dup.dup()
        self.assertTrue(len(dup.threads) <= 5)

    def test_str(self):
        out = str(self.trace)
        self.assertTrue(('Java stacktrace with %d threads' % threads_expected) in out)

class TestJavaThread(unittest.TestCase):
    def setUp(self):
        self.thread = satyr.JavaStacktrace(contents).threads[0]

class TestJavaSharedlib(unittest.TestCase):
    def setUp(self):
        self.shlib = satyr.JavaStacktrace(contents).libs[0]

class TestJavaFrame(unittest.TestCase):
    def setUp(self):
        self.frame = satyr.JavaStacktrace(contents).threads[0].frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertTrue('org.hibernate.exception.ConstraintViolationException' in out)
        self.assertTrue('could not insert' in out)
        self.assertTrue('com.example.myproject' in out)

    def test_dup(self):
        dup = self.frame.dup()
        self.assertEqual(dup.get_name(),
            self.frame.get_name())

        dup.set_name('other')
        self.assertNotEqual(dup.get_name(),
            self.frame.get_name())

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertEqual(dup.cmp(dup), 0)
        self.assertEqual(dup.cmp(self.frame), 0)
        self.assertEqual(dup.cmp(self.frame), 0)
        dup.set_name('another')
        self.assertNotEqual(dup.cmp(self.frame), 0)

if __name__ == '__main__':
    unittest.main()
