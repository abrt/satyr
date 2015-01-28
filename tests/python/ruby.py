#!/usr/bin/env python

import unittest
from test_helpers import *

path = '../ruby_stacktraces/ruby-01'
contents = load_input_contents(path)
frames_expected = 21
expected_short_text = '''#1 rescue in block (2 levels) in func in /usr/share/ruby/vendor_ruby/will_crash.rb:13
#2 block (2 levels) in func in /usr/share/ruby/vendor_ruby/will_crash.rb:10
#3 times in /usr/share/ruby/vendor_ruby/will_crash.rb:9
#4 block in func in /usr/share/ruby/vendor_ruby/will_crash.rb:9
#5 times in /usr/share/ruby/vendor_ruby/will_crash.rb:8
#6 func in /usr/share/ruby/vendor_ruby/will_crash.rb:8
'''

class TestRubyStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.RubyStacktrace(contents)

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
        self.assertTrue(('Ruby stacktrace with %d frames' % frames_expected) in out)

    def test_getset(self):
        self.assertGetSetCorrect(self.trace, 'exception_name', 'Wrap::MyException', 'WhateverException')

    def test_special_functions(self):
        trace = load_input_contents('../ruby_stacktraces/ruby-01')
        trace = satyr.RubyStacktrace(trace)

        f = trace.frames[0]
        self.assertEqual(f.file_name, '/usr/share/ruby/vendor_ruby/will_crash.rb')
        self.assertEqual(f.function_name, 'func')
        self.assertFalse(f.special_function)
        self.assertEqual(f.block_level, 2)
        self.assertEqual(f.rescue_level, 1)

        f = trace.frames[-1]
        self.assertEqual(f.file_name, '/usr/bin/will_ruby_raise')
        self.assertEqual(f.function_name, 'main')
        self.assertTrue(f.special_function)
        self.assertEqual(f.block_level, 0)
        self.assertEqual(f.rescue_level, 0)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(6), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), '6124e03542a0381b14ebaf2c5b5e9f467ebba33b')

    def test_duphash(self):
        expected_plain = '''Thread
/usr/share/ruby/vendor_ruby/will_crash.rb:13
/usr/share/ruby/vendor_ruby/will_crash.rb:10
/usr/share/ruby/vendor_ruby/will_crash.rb:9
'''
        self.assertEqual(self.trace.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.trace.get_duphash(), 'c877cda04fbce8d51c4d9b1d628b0f618677607e')

    def test_crash_thread(self):
        self.assertTrue(self.trace.crash_thread is self.trace)

    def test_from_json(self):
        trace = satyr.RubyStacktrace.from_json('{}')
        self.assertEqual(trace.frames, [])

        json_text = '''
        {
            "exception_name": "NotImplementedError",
            "stacktrace": [
                {
                    "file_name": "/usr/share/foobar/mod/file.rb",
                    "function_name": "do_nothing",
                    "file_line": 42,
                },
                {
                    "special_function": "top (required)",
                    "file_line": 451
                },
                {
                    "unknown_key": 19876543,
                    "block_level": 42,
                    "rescue_level": 6
                }
            ]
        }
'''
        trace = satyr.RubyStacktrace.from_json(json_text)
        self.assertEqual(trace.exception_name, 'NotImplementedError')
        self.assertEqual(len(trace.frames), 3)

        self.assertEqual(trace.frames[0].file_name, '/usr/share/foobar/mod/file.rb')
        self.assertEqual(trace.frames[0].function_name, 'do_nothing')
        self.assertEqual(trace.frames[0].file_line, 42)
        self.assertFalse(trace.frames[0].special_function)
        self.assertEqual(trace.frames[0].block_level, 0)
        self.assertEqual(trace.frames[0].rescue_level, 0)

        self.assertEqual(trace.frames[1].file_name, None)
        self.assertEqual(trace.frames[1].function_name, 'top (required)')
        self.assertEqual(trace.frames[1].file_line, 451)
        self.assertTrue(trace.frames[1].special_function)
        self.assertEqual(trace.frames[1].block_level, 0)
        self.assertEqual(trace.frames[1].rescue_level, 0)

        self.assertEqual(trace.frames[2].file_name, None)
        self.assertEqual(trace.frames[2].function_name, None)
        self.assertEqual(trace.frames[2].file_line, 0)
        self.assertFalse(trace.frames[2].special_function)
        self.assertEqual(trace.frames[2].block_level, 42)
        self.assertEqual(trace.frames[2].rescue_level, 6)

    def test_hash(self):
        self.assertHashable(self.trace)


class TestRubyFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.RubyStacktrace(contents).frames[1]

    def test_str(self):
        out = str(self.frame)
        self.assertEqual(out, "/usr/share/ruby/vendor_ruby/will_crash.rb:10:in `block (2 levels) in func'")

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
        self.assertGetSetCorrect(self.frame, 'file_name', '/usr/share/ruby/vendor_ruby/will_crash.rb', 'java.rs')
        self.assertGetSetCorrect(self.frame, 'file_line', 10, 6667)
        self.assertGetSetCorrect(self.frame, 'special_function', False, True)
        self.assertGetSetCorrect(self.frame, 'function_name', 'func', 'iiiiii')
        self.assertGetSetCorrect(self.frame, 'block_level', 2, 666)
        self.assertGetSetCorrect(self.frame, 'rescue_level', 0, 9000)

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
