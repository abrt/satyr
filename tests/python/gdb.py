#!/usr/bin/env python

import unittest
from test_helpers import *

contents = load_input_contents('../gdb_stacktraces/rhbz-803600')
threads_expected = 2
frames_expected = 227
expected_short_text = '''Thread no. 1 (5 frames)
 #0 validate_row at gtktreeview.c:5699
 #1 validate_visible_area at gtktreeview.c:5898
 #2 gtk_tree_view_bin_expose at gtktreeview.c:4253
 #3 gtk_tree_view_expose at gtktreeview.c:4955
 #4 _gtk_marshal_BOOLEAN__BOXED at gtkmarshalers.c:84
'''

expected_short_text_955617 = '''Thread no. 1 (3 frames)
 #10 xf86CursorSetCursor at xf86Cursor.c:333
 #11 xf86CursorEnableDisableFBAccess at xf86Cursor.c:233
 #12 ?? at /usr/lib/xorg/modules/drivers/nvidia_drv.so
'''

class TestGdbStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.GdbStacktrace(contents)

    def test_correct_thread_count(self):
        self.assertEqual(len(self.trace.threads), threads_expected)

    def test_correct_frame_count(self):
        self.assertEqual(frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(id(dup.threads), id(self.trace.threads))
        self.assertTrue(all(map(lambda t1, t2: t1.equals(t2), dup.threads, self.trace.threads)))

        dup.threads = dup.threads[:5]
        dup2 = dup.dup()
        self.assertNotEqual(id(dup.threads), id(dup2.threads))
        self.assertTrue(all(map(lambda t1, t2: t1.equals(t2), dup.threads, dup2.threads)))

    def test_prepare_linked_list(self):
        dup = self.trace.dup()
        dup.threads = dup.threads[:5]
        dup.normalize()
        self.assertTrue(len(dup.threads) <= 5)

    def test_normalize(self):
        dup = self.trace.dup()
        dup.normalize()
        self.assertNotEqual(frame_count(dup), frame_count(self.trace))

    def test_str(self):
        out = str(self.trace)
        self.assertTrue(('Stacktrace with %d threads' % threads_expected) in out)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(5), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), 'd0fcdc87161ccb093f7efeff12218321d8fd5298')

    def test_crash_thread(self):
        self.assertTrue(self.trace.crash_thread is self.trace.threads[1])

    def test_hash(self):
        self.assertHashable(self.trace)

    def test_short_text_normalization(self):
        contents = load_input_contents('../gdb_stacktraces/rhbz-955617')
        trace = satyr.GdbStacktrace(contents)
        self.assertEqual(trace.to_short_text(5), expected_short_text_955617)

class TestGdbThread(BindingsTestCase):
    def setUp(self):
        self.thread = satyr.GdbStacktrace(contents).threads[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.thread, 'number', 2, 9000)

    def test_equals(self):
        self.assertTrue(self.thread.equals(self.thread))
        dup = self.thread.dup()
        self.assertTrue(self.thread.equals(dup))
        dup.number = 9000
        self.assertFalse(self.thread.equals(dup))

    def test_duphash(self):
        expected_plain = 'Thread\n  write\n  virNetSocketWriteWire\n  virNetSocketWrite\n'
        self.assertEqual(self.thread.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.thread.get_duphash(), '01d2a92281954a81dee9098dc4f8056ef5a5a5e1')

    def test_hash(self):
        self.assertHashable(self.thread)

class TestGdbSharedlib(BindingsTestCase):
    def setUp(self):
        self.shlib = satyr.GdbStacktrace(contents).libs[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.shlib, 'start_address', 0x3ecd63c680, 10)
        self.assertGetSetCorrect(self.shlib, 'end_address', 0x3ecd71f0f8, 20)
        self.assertGetSetCorrect(self.shlib, 'symbols', satyr.SYMS_OK, satyr.SYMS_WRONG)
        self.assertGetSetCorrect(self.shlib, 'soname', '/usr/lib64/libpython2.6.so.1.0', '/dev/null')

    def test_hash(self):
        self.assertHashable(self.shlib)

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
        self.assertTrue(dup.equals(dup))
        self.assertTrue(dup.equals(self.frame))
        dup.function_name = 'another'
        self.assertFalse(dup.equals(self.frame))

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'function_name', 'write', 'foo bar')
        self.assertGetSetCorrect(self.frame, 'function_type', None, 'Maybe Integer')
        self.assertGetSetCorrect(self.frame, 'number', 0, 42)
        self.assertGetSetCorrect(self.frame, 'source_file', '../sysdeps/unix/syscall-template.S', 'ok.c')
        self.assertGetSetCorrect(self.frame, 'source_line', 82, 1337)
        self.assertGetSetCorrect(self.frame, 'signal_handler_called', False, True)
        self.assertGetSetCorrect(self.frame, 'address', 0x3ec220e48d, 0x666)
        self.assertGetSetCorrect(self.frame, 'address', 0x666, 4398046511104)
        ## 2^66, this is expected to fail
        #self.assertGetSetCorrect(self.frame, 'address', 4398046511104, 73786976294838206464L)
        self.assertGetSetCorrect(self.frame, 'library_name', None, 'sowhat.so')

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
