#!/usr/bin/env python

import unittest

from test_helpers import BindingsTestCase, load_input_contents

try:
    import _satyr as satyr
except ImportError:
    import satyr

path = '../python_stacktraces/python-01'
contents = load_input_contents(path)
frames_expected = 11
expected_short_text = '''#1 _getPackage in /usr/share/PackageKit/helpers/yum/yumBackend.py:2534
#2 updateProgress in /usr/share/PackageKit/helpers/yum/yumBackend.py:2593
#3 _do_start in /usr/share/PackageKit/helpers/yum/yumBackend.py:2551
#4 start in /usr/lib/python2.6/site-packages/urlgrabber/progress.py:129
#5 downloadPkgs in /usr/lib/yum-plugins/presto.py:419
#6 predownload_hook in /usr/lib/yum-plugins/presto.py:577
'''

class TestPythonStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.PythonStacktrace(contents)

    def frame_count(self, trace):
        return len(trace.frames)

    def test_correct_frame_count(self):
        self.assertEqual(self.frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(id(dup.frames), id(self.trace.frames))
        self.assertEqual(dup.frames, self.trace.frames)

        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertEqual(len(dup.frames), len(dup2.frames))
        self.assertNotEqual(id(dup.frames), id(dup2.frames))

    def test_prepare_linked_list(self):
        dup = self.trace.dup()
        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertTrue(len(dup2.frames) <= 5)

    def test_str(self):
        out = str(self.trace)
        self.assertTrue(('Python stacktrace with %d frames' % frames_expected) in out)

    def test_getset(self):
        self.assertGetSetCorrect(self.trace, 'exception_name', 'AttributeError', 'WhateverException')

    def test_special_files_and_frames(self):
        trace = load_input_contents('../python_stacktraces/python-03')
        trace = satyr.PythonStacktrace(trace)

        f = trace.frames[0]
        self.assertEqual(f.file_name, 'string')
        self.assertEqual(f.function_name, 'module')
        self.assertTrue(f.special_file)
        self.assertTrue(f.special_function)

        f = trace.frames[1]
        self.assertEqual(f.file_name, './test.py')
        self.assertEqual(f.function_name, 'f')
        self.assertFalse(f.special_file)
        self.assertFalse(f.special_function)

        f = trace.frames[2]
        self.assertEqual(f.file_name, './test.py')
        self.assertEqual(f.function_name, 'lambda')
        self.assertFalse(f.special_file)
        self.assertTrue(f.special_function)

        f = trace.frames[3]
        self.assertEqual(f.file_name, 'stdin')
        self.assertEqual(f.function_name, 'module')
        self.assertTrue(f.special_file)
        self.assertTrue(f.special_function)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(6), expected_short_text)

class TestPythonFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.PythonStacktrace(contents).frames[-1]

    def test_str(self):
        out = str(self.frame)
        self.assertTrue('yumBackend.py' in out)
        self.assertTrue('1830' in out)
        self.assertTrue('_runYumTransaction' in out)

    def test_dup(self):
        dup = self.frame.dup()
        self.assertEqual(dup.function_name,
            self.frame.function_name)

        dup.function_name = 'other'
        self.assertNotEqual(dup.function_name,
            self.frame.function_name)

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertEqual(dup, dup)
        self.assertEqual(dup, self.frame)
        self.assertEqual(dup, self.frame)
        self.assertNotEqual(id(dup), id(self.frame))
        dup.function_name = 'another'
        self.assertNotEqual(dup, self.frame)
        self.assertTrue(dup > self.frame)

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'file_name', '/usr/share/PackageKit/helpers/yum/yumBackend.py', 'java.py')
        self.assertGetSetCorrect(self.frame, 'file_line', 1830, 6667)
        self.assertGetSetCorrect(self.frame, 'special_file', False, True)
        self.assertGetSetCorrect(self.frame, 'special_function', False, True)
        self.assertGetSetCorrect(self.frame, 'function_name', '_runYumTransaction', 'iiiiii')
        self.assertGetSetCorrect(self.frame, 'line_contents', 'rpmDisplay=rpmDisplay)', 'abcde')

if __name__ == '__main__':
    unittest.main()
