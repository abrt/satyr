#!/usr/bin/env python

import unittest
from test_helpers import *

path = '../js_stacktraces/node-01'
contents = load_input_contents(path)
frames_expected = 10
expected_short_text = '#1 at Object.<anonymous> (/tmp/index.js:2:1)\n#2 at Module._compile (module.js:556:32)\n#3 at Object.Module._extensions..js (module.js:565:10)\n#4 at Module.load (module.js:473:32)\n#5 at tryModuleLoad (module.js:432:12)\n#6 at Function.Module._load (module.js:424:3)\n'

class TestJavaScriptStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.JavaScriptStacktrace(contents)

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
        self.assertTrue(('JavaScript stacktrace with %d frames' % frames_expected) in out)

    def test_getset(self):
        self.assertGetSetCorrect(self.trace, 'exception_name', 'ReferenceError', 'WhateverError')

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(6), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), 'f4b36e13fd620fc6c650241b5753f4629397c7d6')

    def test_duphash(self):
        expected_plain = '''Thread
/tmp/index.js:2
module.js:556
module.js:565
'''

        self.assertEqual(self.trace.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.trace.get_duphash(), 'e3c6aae82e127c2748a36fc3eec7c5eb41493fd3')

    def test_crash_thread(self):
        self.assertTrue(self.trace.crash_thread is self.trace)

    def test_from_json(self):
        trace = satyr.JavaScriptStacktrace.from_json('{}')
        self.assertEqual(trace.frames, [])

        json_text = '''
        {
            "exception_name": "ReferenceError",
            "stacktrace": [
                {
                    "file_name": "bar.js",
                    "function_name": "do_nothing",
                    "file_line": 42,
                    "line_column": 24,
                },
                {
                    "file_name": "foo.js",
                    "function_name": "do_everything",
                    "file_line": 21,
                    "line_column": 12,
                },
            ]
        }
'''
        trace = satyr.JavaScriptStacktrace.from_json(json_text)
        self.assertEqual(trace.exception_name, 'ReferenceError')
        self.assertEqual(len(trace.frames), 2)

        self.assertEqual(trace.frames[0].file_name, "bar.js")
        self.assertEqual(trace.frames[0].function_name, 'do_nothing')
        self.assertEqual(trace.frames[0].file_line, 42)
        self.assertEqual(trace.frames[0].line_column, 24)

        self.assertEqual(trace.frames[1].file_name, "foo.js")
        self.assertEqual(trace.frames[1].function_name, 'do_everything')
        self.assertEqual(trace.frames[1].file_line, 21)
        self.assertEqual(trace.frames[1].line_column, 12)

    def test_hash(self):
        self.assertHashable(self.trace)


class TestJavaScriptFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.JavaScriptStacktrace(contents).frames[1]

    def test_str(self):
        out = str(self.frame)
        self.assertEqual(out, "at Module._compile (module.js:556:32)")

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
        self.assertGetSetCorrect(self.frame, 'file_name', 'module.js', '/usr/lib/npm/modules/bar/foo.js')
        self.assertGetSetCorrect(self.frame, 'file_line', 556, 6667)
        self.assertGetSetCorrect(self.frame, 'line_column', 32, 6669)
        self.assertGetSetCorrect(self.frame, 'function_name', 'Module._compile', 'iiiiii')

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
