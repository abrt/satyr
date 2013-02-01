#!/usr/bin/env python

import os
import unittest

try:
    import _satyr as satyr
except ImportError:
    import satyr

path = '../python_stacktraces/python-01'
frames_expected = 11

if not os.path.isfile(path):
    path = '../' + path

with file(path) as f:
    contents = f.read()

class TestPythonStacktrace(unittest.TestCase):
    def setUp(self):
        self.trace = satyr.PythonStacktrace(contents)

    def frame_count(self, trace):
        return len(trace.frames)

    def test_correct_frame_count(self):
        self.assertEqual(self.frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(dup.frames, self.trace.frames)

        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertEqual(len(dup.frames), len(dup2.frames))

    def test_prepare_linked_list(self):
        dup = self.trace.dup()
        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertTrue(len(dup2.frames) <= 5)

    def test_str(self):
        out = str(self.trace)
        self.assertIn('Python stacktrace with %d frames' % frames_expected, out)

class TestPythonFrame(unittest.TestCase):
    def setUp(self):
        self.frame = satyr.PythonStacktrace(contents).frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertIn('yumBackend.py', out)
        self.assertIn('1830', out)
        self.assertIn('_runYumTransaction', out)

    def test_dup(self):
        dup = self.frame.dup()
        self.assertEqual(dup.get_function_name(),
            self.frame.get_function_name())

        dup.set_function_name('other')
        self.assertNotEqual(dup.get_function_name(),
            self.frame.get_function_name())

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertEqual(dup.cmp(dup), 0)
        self.assertEqual(dup.cmp(self.frame), 0)
        self.assertEqual(dup.cmp(self.frame), 0)
        dup.set_function_name('another')
        self.assertNotEqual(dup.cmp(self.frame), 0)

if __name__ == '__main__':
    unittest.main()
