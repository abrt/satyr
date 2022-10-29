#!/usr/bin/env python

import unittest
from test_helpers import *

path = '../python_stacktraces/python-01'
contents = load_input_contents(path)
frames_expected = 11
expected_short_text = '''#1 [/home/anonymized/PackageKit/helpers/yum/yumBackend.py:2534] _getPackage
#2 [/usr/share/PackageKit/helpers/yum/yumBackend.py:2593] updateProgress
#3 [/usr/share/PackageKit/helpers/yum/yumBackend.py:2551] _do_start
#4 [/usr/lib/python2.6/site-packages/urlgrabber/progress.py:129] start
#5 [/usr/lib/yum-plugins/presto.py:419] downloadPkgs
#6 [/usr/lib/yum-plugins/presto.py:577] predownload_hook
'''

class TestPythonStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.PythonStacktrace(contents)

    def test_correct_frame_count(self):
        self.assertEqual(frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(id(dup.frames), id(self.trace.frames))
        self.assertTrue(all(map(lambda t1, t2: t1.equals(t2), dup.frames, self.trace.frames)))

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

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), 'eabeeae89433bb3b3d9eb8190659dcf057ab3cd1')

    def test_duphash(self):
        expected_plain = '''Thread
/home/anonymized/PackageKit/helpers/yum/yumBackend.py:2534
/usr/share/PackageKit/helpers/yum/yumBackend.py:2593
/usr/share/PackageKit/helpers/yum/yumBackend.py:2551
'''
        self.assertEqual(self.trace.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.trace.get_duphash(), '8c8273cddf94e10fc0349284afcff8970056d9e5')

    def test_crash_thread(self):
        self.assertTrue(self.trace.crash_thread is self.trace)

    def test_from_json(self):
        trace = satyr.PythonStacktrace.from_json('{}')
        self.assertEqual(trace.frames, [])

        json_text = '''
        {
            "exception_name": "NotImplementedError",
            "stacktrace": [
                {
                    "file_name": "/usr/share/foobar/mod/file.py",
                    "function_name": "do_nothing",
                    "file_line": 42,
                    "line_contents": "#this does nothing"
                },
                {
                    "special_file": "stdin",
                    "special_function": "module",
                    "file_line": 451
                },
                {
                    "unknown_key": 19876543
                }
            ]
        }
'''
        trace = satyr.PythonStacktrace.from_json(json_text)
        self.assertEqual(trace.exception_name, 'NotImplementedError')
        self.assertEqual(len(trace.frames), 3)

        self.assertEqual(trace.frames[0].file_name, '/usr/share/foobar/mod/file.py')
        self.assertEqual(trace.frames[0].function_name, 'do_nothing')
        self.assertEqual(trace.frames[0].file_line, 42)
        self.assertEqual(trace.frames[0].line_contents, '#this does nothing')
        self.assertFalse(trace.frames[0].special_file)
        self.assertFalse(trace.frames[0].special_function)

        self.assertEqual(trace.frames[1].file_name, 'stdin')
        self.assertEqual(trace.frames[1].function_name, 'module')
        self.assertEqual(trace.frames[1].file_line, 451)
        self.assertEqual(trace.frames[1].line_contents, None)
        self.assertTrue(trace.frames[1].special_file)
        self.assertTrue(trace.frames[1].special_function)

        self.assertEqual(trace.frames[2].file_name, None)
        self.assertEqual(trace.frames[2].function_name, None)
        self.assertEqual(trace.frames[2].file_line, 0)
        self.assertEqual(trace.frames[2].line_contents, None)
        self.assertFalse(trace.frames[2].special_file)
        self.assertFalse(trace.frames[2].special_function)

    def test_hash(self):
        self.assertHashable(self.trace)

    def test_invalid_syntax_current_file(self):
        trace = load_input_contents('../python_stacktraces/python-04')
        trace = satyr.PythonStacktrace(trace)

        self.assertEqual(len(trace.frames), 1)
        self.assertEqual(trace.exception_name, 'SyntaxError')

        f = trace.frames[0]
        self.assertEqual(f.file_name, '/usr/bin/python3-mako-render')
        self.assertEqual(f.function_name, "syntax")
        self.assertEqual(f.file_line, 43)
        self.assertEqual(f.line_contents, 'print render(data, kw)')
        self.assertTrue(f.special_function)
        self.assertFalse(f.special_file)

    def test_invalid_syntax_imported_file(self):
        trace = load_input_contents('../python_stacktraces/python-05')
        trace = satyr.PythonStacktrace(trace)

        self.assertEqual(len(trace.frames), 2)
        self.assertEqual(trace.exception_name, 'SyntaxError')

        f = trace.frames[1]
        self.assertEqual(f.file_name, '/usr/bin/will_python_raise')
        self.assertEqual(f.function_name, "module")
        self.assertEqual(f.file_line, 2)
        self.assertEqual(f.line_contents, 'import report')
        self.assertTrue(f.special_function)
        self.assertFalse(f.special_file)

        f = trace.frames[0]
        self.assertEqual(f.file_name, '/usr/lib64/python2.7/site-packages/report/__init__.py')
        self.assertEqual(f.function_name, "syntax")
        self.assertEqual(f.file_line, 15)
        self.assertEqual(f.line_contents, 'def foo(:')
        self.assertTrue(f.special_function)
        self.assertFalse(f.special_file)

    def test_fine_grained_error_location(self):
        trace = load_input_contents('../python_stacktraces/python-06')
        trace = satyr.PythonStacktrace(trace)

        self.assertEqual(len(trace.frames), 1)
        self.assertEqual(trace.exception_name, 'ZeroDivisionError')

        f = trace.frames[0]
        self.assertEqual(f.file_name, '/usr/bin/will_python3_raise')
        self.assertEqual(f.function_name, "module")
        self.assertEqual(f.file_line, 3)
        self.assertEqual(f.line_contents, '0/0')
        self.assertTrue(f.special_function)
        self.assertFalse(f.special_file)


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
        self.assertTrue(dup.equals(dup))
        self.assertTrue(dup.equals(self.frame))
        self.assertNotEqual(id(dup), id(self.frame))
        dup.function_name = 'another'
        self.assertFalse(dup.equals(self.frame))

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'file_name', '/usr/share/PackageKit/helpers/yum/yumBackend.py', 'java.py')
        self.assertGetSetCorrect(self.frame, 'file_line', 1830, 6667)
        self.assertGetSetCorrect(self.frame, 'special_file', False, True)
        self.assertGetSetCorrect(self.frame, 'special_function', False, True)
        self.assertGetSetCorrect(self.frame, 'function_name', '_runYumTransaction', 'iiiiii')
        self.assertGetSetCorrect(self.frame, 'line_contents', 'rpmDisplay=rpmDisplay)', 'abcde')

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
