#!/usr/bin/env python

import unittest
from test_helpers import BindingsTestCase, load_input_contents

try:
    import _satyr as satyr
except ImportError:
    import satyr


contents = load_input_contents('../gdb_stacktraces/rhbz-803600')
threads_expected = 2
frames_expected = 227
expected_short_text = '''Thread no. 1 (5 frames)
 #0 validate_row at gtktreeview.c
 #1 validate_visible_area at gtktreeview.c
 #2 gtk_tree_view_bin_expose at gtktreeview.c
 #3 gtk_tree_view_expose at gtktreeview.c
 #4 _gtk_marshal_BOOLEAN__BOXED at gtkmarshalers.c
'''

class TestGdbStacktrace(BindingsTestCase):
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
        self.assertNotEqual(id(dup.threads), id(self.trace.threads))
        self.assertEqual(dup.threads, self.trace.threads)

        dup.threads = dup.threads[:5]
        dup2 = dup.dup()
        self.assertNotEqual(id(dup.threads), id(dup2.threads))
        self.assertEqual(dup.threads, dup2.threads)

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
        self.assertTrue(('Stacktrace with %d threads' % threads_expected) in out)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(5), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), 'd0fcdc87161ccb093f7efeff12218321d8fd5298')

class TestGdbThread(BindingsTestCase):
    def setUp(self):
        self.thread = satyr.GdbStacktrace(contents).threads[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.thread, 'number', 2, 9000)

    def test_cmp(self):
        self.assertEqual(self.thread, self.thread)
        dup = self.thread.dup()
        self.assertEqual(self.thread, dup)
        dup.number = 9000
        self.assertNotEqual(self.thread, dup)

class TestGdbSharedlib(BindingsTestCase):
    def setUp(self):
        self.shlib = satyr.GdbStacktrace(contents).libs[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.shlib, 'start_address', 0x3ecd63c680, 10)
        self.assertGetSetCorrect(self.shlib, 'end_address', 0x3ecd71f0f8, 20)
        self.assertGetSetCorrect(self.shlib, 'symbols', satyr.SYMS_OK, satyr.SYMS_WRONG)
        self.assertGetSetCorrect(self.shlib, 'soname', '/usr/lib64/libpython2.6.so.1.0', '/dev/null')

class TestGdbFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.GdbStacktrace(contents).threads[0].frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertTrue('0x0000003ec220e48d' in out)
        self.assertTrue('write' in out)
        self.assertTrue('Frame #0' in out)

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
        dup.function_name = 'another'
        self.assertNotEqual(dup, self.frame)

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'function_name', 'write', 'foo bar')
        self.assertGetSetCorrect(self.frame, 'function_type', None, 'Maybe Integer')
        self.assertGetSetCorrect(self.frame, 'number', 0, 42)
        self.assertGetSetCorrect(self.frame, 'source_file', '../sysdeps/unix/syscall-template.S', 'ok.c')
        self.assertGetSetCorrect(self.frame, 'source_line', 82, 1337)
        self.assertGetSetCorrect(self.frame, 'signal_handler_called', False, True)
        self.assertGetSetCorrect(self.frame, 'address', 0x3ec220e48d, 0x666)
        self.assertGetSetCorrect(self.frame, 'address', 0x666, 4398046511104L)
        ## 2^66, this is expected to fail
        #self.assertGetSetCorrect(self.frame, 'address', 4398046511104L, 73786976294838206464L)
        self.assertGetSetCorrect(self.frame, 'library_name', None, 'sowhat.so')

if __name__ == '__main__':
    unittest.main()
