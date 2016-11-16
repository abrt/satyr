#!/usr/bin/env python

import unittest
from test_helpers import *

contents = load_input_contents('../json_files/core-01')
threads_expected = 2
frames_expected = 6
expected_short_text = '''#1 [libc-2.15.so] raise
#2 [libc-2.15.so] abort
#3 [will_abort] 92ebaf825e4f492952c45189cb9ffc6541f8599b+1123
#4 [libc-2.15.so] __libc_start_main
'''

class TestCoreStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.CoreStacktrace(contents)

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
        self.assertEqual(len(dup.threads), len(dup2.threads))
        self.assertNotEqual(id(dup.threads), id(dup2.threads))

    def test_prepare_linked_list(self):
        dup = self.trace.dup()
        dup.threads = dup.threads[:5]
        dup2 = dup.dup()
        self.assertTrue(len(dup.threads) <= 5)

    def test_str(self):
        out = str(self.trace)
        self.assertTrue(('Core stacktrace with %d threads' % threads_expected) in out)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(8), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), '6ebee2edb486ee24a3280c18b0db5353bbc22014')

    def test_getset(self):
        self.assertGetSetCorrect(self.trace, 'signal', 6, 42)
        self.assertGetSetCorrect(self.trace, 'executable', '/usr/bin/will_abort', '/bin/true')
        self.assertGetSetCorrect(self.trace, 'only_crash_thread', False, True)

    def test_crash_thread(self):
        self.assertTrue(self.trace.crash_thread is self.trace.threads[1])

    def test_crash_thread_refcount(self):
        c = self.trace.crash_thread
        del c
        c = self.trace.crash_thread
        del c
        # segfault in revision 6f91aba95397e56c665c420f574d73adb64df832
        f = self.trace.crash_thread.frames[0]

    def test_from_json(self):
        trace = satyr.CoreStacktrace.from_json('{}')
        self.assertEqual(trace.threads, [])

        json_text = '''
        {
            "signal": 9,
            "executable": "C:\\\\porn.bat",
            "only_crash_thread": true,
            "stacktrace": [
                {
                    "frames": [
                        {
                            "address": 60100,
                            "build_id": "aaaaaaaaaaaaaaaaaaaaaaaaAAAAAAAAAAA",
                            "build_id_offset": 5,
                            "function_name": "destroy_universe",
                            "file_name": "del.exe",
                            "fingerprint": "1234123412341234123412341234",
                            "fingerprint_hashed": true,
                        },
                        {},
                        {
                            "address": 2,
                            "fingerprint": "a b c d",
                            "fingerprint_hashed": false,
                        }
                    ]
                },
                {
                    "crash_thread": true,
                    "bogus": "value",
                    "frames": [
                        {
                            "address": 1,
                            "build_id": "b",
                            "build_id_offset": 5,
                        }
                    ]
                }
            ]
        }
'''
        trace = satyr.CoreStacktrace.from_json(json_text)
        self.assertEqual(trace.signal, 9)
        self.assertEqual(trace.executable, 'C:\\porn.bat')
        self.assertEqual(trace.only_crash_thread, True)

        self.assertEqual(len(trace.threads), 2)
        self.assertEqual(len(trace.threads[0].frames), 3)
        self.assertEqual(len(trace.threads[1].frames), 1)

        self.assertEqual(trace.threads[0].frames[0].address, 60100)
        self.assertEqual(trace.threads[0].frames[0].build_id, 'aaaaaaaaaaaaaaaaaaaaaaaaAAAAAAAAAAA')
        self.assertEqual(trace.threads[0].frames[0].build_id_offset, 5)
        self.assertEqual(trace.threads[0].frames[0].function_name, 'destroy_universe')
        self.assertEqual(trace.threads[0].frames[0].file_name, 'del.exe')
        self.assertEqual(trace.threads[0].frames[0].fingerprint, '1234123412341234123412341234')
        self.assertTrue(trace.threads[0].frames[0].fingerprint_hashed)

        self.assertEqual(trace.threads[0].frames[1].address, None)
        self.assertEqual(trace.threads[0].frames[1].build_id, None)
        self.assertEqual(trace.threads[0].frames[1].build_id_offset, None)
        self.assertEqual(trace.threads[0].frames[1].function_name, None)
        self.assertEqual(trace.threads[0].frames[1].file_name, None)
        self.assertEqual(trace.threads[0].frames[1].fingerprint, None)
        self.assertTrue(trace.threads[0].frames[1].fingerprint_hashed)

        self.assertEqual(trace.threads[0].frames[2].address, 2)
        self.assertEqual(trace.threads[0].frames[2].build_id, None)
        self.assertEqual(trace.threads[0].frames[2].build_id_offset, None)
        self.assertEqual(trace.threads[0].frames[2].function_name, None)
        self.assertEqual(trace.threads[0].frames[2].file_name, None)
        self.assertEqual(trace.threads[0].frames[2].fingerprint, 'a b c d')
        self.assertFalse(trace.threads[0].frames[2].fingerprint_hashed)

        self.assertEqual(trace.threads[1].frames[0].address, 1)
        self.assertEqual(trace.threads[1].frames[0].build_id, 'b')
        self.assertEqual(trace.threads[1].frames[0].build_id_offset, 5)
        self.assertEqual(trace.threads[1].frames[0].function_name, None)
        self.assertEqual(trace.threads[1].frames[0].file_name, None)
        self.assertEqual(trace.threads[1].frames[0].fingerprint, None)
        self.assertTrue(trace.threads[1].frames[0].fingerprint_hashed)

    def test_hash(self):
        self.assertHashable(self.trace)

class TestCoreThread(BindingsTestCase):
    def setUp(self):
        self.thread = satyr.CoreStacktrace(contents).threads[0]

    def test_cmp(self):
        self.assertEqual(self.thread, self.thread)
        dup = self.thread.dup()
        self.assertTrue(self.thread.equals(dup))
        dup.frames[0].build_id = 'wut'
        self.assertFalse(self.thread.equals(dup))

    def test_duphash(self):
        expected_plain = '''Thread
92ebaf825e4f492952c45189cb9ffc6541f8599b+0x2a
92ebaf825e4f492952c45189cb9ffc6541f8599b+0x29ee
'''
        self.assertEqual(self.thread.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.thread.get_duphash(), 'ad8486fa45ff39ffed7a07f3b68b8f406ebe2550')

    def test_hash(self):
        self.assertHashable(self.thread)

class TestCoreFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.CoreStacktrace(contents).threads[1].frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertEquals(out, '[0x0000003739a35935] raise cc10c72da62c93033e227ffbe2670f2c4fbbde1a+0x35935 '
                               '[/usr/lib64/libc-2.15.so] '
                               'fingerprint: f33186a4c862fb0751bca60701f553b829210477 (hashed)')

    def test_dup(self):
        dup = self.frame.dup()
        self.assertTrue(dup.equals(self.frame))

        dup.function_name = 'other'
        self.assertFalse(dup.equals(self.frame))

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertTrue(dup.equals(dup))
        self.assertTrue(dup.equals(self.frame))
        self.assertNotEqual(id(dup), id(self.frame))
        dup.function_name = 'another'
        self.assertFalse(dup.equals(self.frame))

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'address', 237190207797, 5000)
        self.assertGetSetCorrect(self.frame, 'build_id', 'cc10c72da62c93033e227ffbe2670f2c4fbbde1a', 'abcdef')
        self.assertGetSetCorrect(self.frame, 'build_id_offset', 219445, 44)
        self.assertGetSetCorrect(self.frame, 'function_name', 'raise', 'lower')
        self.assertGetSetCorrect(self.frame, 'file_name', '/usr/lib64/libc-2.15.so', '/u/l/x')
        self.assertGetSetCorrect(self.frame, 'fingerprint', 'f33186a4c862fb0751bca60701f553b829210477', 'xxx')
        self.assertGetSetCorrect(self.frame, 'fingerprint_hashed', True, False)

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
