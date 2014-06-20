#!/usr/bin/env python

import unittest
from test_helpers import *

contents = load_input_contents('../kerneloopses/rhbz-827868')
frames_expected = 32
mods_expected = 119
expected_short_text = '''#1 warn_slowpath_common
#2 warn_slowpath_null
#3 __alloc_pages_nodemask
#4 ? ip_copy_metadata
#5 ? ip_forward_options
#6 alloc_pages_current
#7 __get_free_pages
'''

class TestKerneloops(BindingsTestCase):
    def setUp(self):
        self.koops = satyr.Kerneloops(contents)

    def test_correct_frame_count(self):
        self.assertEqual(len(self.koops.frames), frames_expected)

    def test_correct_modules(self):
        self.assertEqual(len(self.koops.modules), mods_expected)
        ## not implemented, expected to fail
        #self.koops.modules = ['foo', 'bar']
        #self.assertEqual(len(self.koops.modules), 2)
        self.assertRaises(NotImplementedError, self.koops.__delattr__, 'modules')

    def test_correct_taint_flags(self):
        for val in self.koops.taint_flags.values():
            self.assertFalse(val);
        contents = load_input_contents('../kerneloopses/rhbz-827868-modified')
        tainted_koops = satyr.Kerneloops(contents)

        true_flags = set([f for (f, v) in tainted_koops.taint_flags.items() if v == True])
        self.assertEqual(true_flags,
            set(['module_proprietary', 'page_release', 'died_recently', 'module_out_of_tree']))

    def test_dup(self):
        dup = self.koops.dup()
        self.assertNotEqual(id(dup.frames), id(self.koops.frames))
        self.assertTrue(all(map(lambda t1, t2: t1.equals(t2), dup.frames, self.koops.frames)))

        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertEqual(len(dup.frames), len(dup2.frames))
        self.assertNotEqual(id(dup.frames), id(dup2.frames))

    def test_normalize(self):
        dup = self.koops.dup()
        dup.normalize()
        self.assertNotEqual(len(dup.frames), len(self.koops.frames))
        self.assertNotEqual(dup.frames, self.koops.frames)

    def test_str(self):
        out = str(self.koops)
        self.assertTrue(('Kerneloops with %d frames' % frames_expected) in out)

    def test_to_short_text(self):
        self.assertEqual(self.koops.to_short_text(7), expected_short_text)

    def test_bthash(self):
        self.assertEqual(self.koops.get_bthash(), '73c7ce83d5ba90a1acbcc7915a62595914321f97')

    def test_duphash(self):
        expected_plain = 'Thread\n__alloc_pages_nodemask\nip_copy_metadata\nip_forward_options\n'
        self.assertEqual(self.koops.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.koops.get_duphash(), '53f62f9d6f7de093f50653863d200f4789ace7ef')
        compat_expected_plain = ('__alloc_pages_nodemask\nalloc_pages_current\n__get_free_pages\n' +
                                 'kmalloc_order_trace\n__kmalloc\npskb_expand_head\n')
        self.assertEqual(self.koops.get_duphash(flags=(satyr.DUPHASH_NOHASH|satyr.DUPHASH_KOOPS_COMPAT), frames=6),
                         compat_expected_plain)
        self.assertEqual(self.koops.get_duphash(flags=satyr.DUPHASH_KOOPS_COMPAT, frames=6),
                         '5718b3a86c64e7bed5e8ead08ae3084e447ddbee')
        self.assertRaises(RuntimeError, self.koops.get_duphash, flags=(satyr.DUPHASH_NOHASH|satyr.DUPHASH_KOOPS_COMPAT), frames=-1)

    def test_crash_thread(self):
        self.assertTrue(self.koops.crash_thread is self.koops)

    def test_from_json(self):
        trace = satyr.Kerneloops.from_json('{}')
        self.assertEqual(trace.frames, [])

        json_text = '''
        {
            "version": "3.11.666",
            "taint_flags": ["warning", "died_recently"],
            "modules": ["not", "much", "modules", "loaded"],
            "frames": [
                {
                    "address": 28,
                    "reliable": true,
                    "function_name": "f",
                    "function_offset": 5,
                    "function_length": 6,
                    "module_name": "modularmodule",
                    "from_address": 27,
                    "from_function_name": "h",
                    "from_function_offset": 3,
                    "from_function_length": 9,
                    "from_module_name": "nonmodularmodule",
                    "byl": "jsem tady"
                },
                {
                },
                {
                    "address": 23,
                    "reliable": false,
                    "function_name": "g",
                    "module_name": "much",
                }
            ]
        }
'''
        trace = satyr.Kerneloops.from_json(json_text)
        self.assertEqual(trace.version, '3.11.666')
        self.assertTrue(trace.taint_flags['warning'])
        self.assertTrue(trace.taint_flags['died_recently'])
        self.assertFalse(trace.taint_flags['module_out_of_tree'])
        self.assertEqual(trace.modules, ['not', 'much', 'modules', 'loaded'])
        self.assertEqual(len(trace.frames), 3)

        self.assertEqual(trace.frames[0].address, 28)
        self.assertTrue(trace.frames[0].reliable)
        self.assertEqual(trace.frames[0].function_name, 'f')
        self.assertEqual(trace.frames[0].function_offset, 5)
        self.assertEqual(trace.frames[0].function_length, 6)
        self.assertEqual(trace.frames[0].module_name, 'modularmodule')
        self.assertEqual(trace.frames[0].from_address, 27)
        self.assertEqual(trace.frames[0].from_function_name, 'h')
        self.assertEqual(trace.frames[0].from_function_offset, 3)
        self.assertEqual(trace.frames[0].from_function_length, 9)
        self.assertEqual(trace.frames[0].from_module_name, 'nonmodularmodule')

        self.assertEqual(trace.frames[1].address, 0)
        self.assertFalse(trace.frames[1].reliable)
        self.assertEqual(trace.frames[1].function_name, None)
        self.assertEqual(trace.frames[1].function_offset, 0)
        self.assertEqual(trace.frames[1].function_length, 0)
        self.assertEqual(trace.frames[1].module_name, None)
        self.assertEqual(trace.frames[1].from_address, 0)
        self.assertEqual(trace.frames[1].from_function_name, None)
        self.assertEqual(trace.frames[1].from_function_offset, 0)
        self.assertEqual(trace.frames[1].from_function_length, 0)
        self.assertEqual(trace.frames[1].from_module_name, None)

        self.assertEqual(trace.frames[2].address, 23)
        self.assertFalse(trace.frames[2].reliable)
        self.assertEqual(trace.frames[2].function_name, 'g')
        self.assertEqual(trace.frames[2].function_offset, 0)
        self.assertEqual(trace.frames[2].function_length, 0)
        self.assertEqual(trace.frames[2].module_name, 'much')
        self.assertEqual(trace.frames[2].from_address, 0)
        self.assertEqual(trace.frames[2].from_function_name, None)
        self.assertEqual(trace.frames[2].from_function_offset, 0)
        self.assertEqual(trace.frames[2].from_function_length, 0)
        self.assertEqual(trace.frames[2].from_module_name, None)

    def test_to_json(self):
        import json

        def check(filename):
            contents = load_input_contents(filename)
            trace = satyr.Kerneloops(contents)

            report = satyr.Report()
            report.report_type = "kerneloops"
            report.stacktrace = trace
            json_report = report.to_json()

            # json parsing, should not raise an exception
            j = json.loads(json_report)

        check('../kerneloopses/github-113')
        check('../kerneloopses/rhbz-827868-modified')

    def test_hash(self):
        self.assertHashable(self.koops)


class TestKoopsFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.Kerneloops(contents).frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertTrue('0xffffffff81057adf' in out)
        self.assertTrue('warn_slowpath_common' in out)
        self.assertTrue('0x7f' in out)
        self.assertTrue('0xc0' in out)

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

        dup.reliable = False
        self.assertFalse(dup.equals(self.frame))

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'reliable', True, False)
        self.assertGetSetCorrect(self.frame, 'address', 0xffffffff81057adfL, 60200)
        self.assertGetSetCorrect(self.frame, 'function_name', 'warn_slowpath_common', 'fun_ction')
        self.assertGetSetCorrect(self.frame, 'function_offset', 0x7f, 890)
        self.assertGetSetCorrect(self.frame, 'function_length', 0xc0, 234)
        self.assertGetSetCorrect(self.frame, 'module_name', None, 'dev_one.ko')
        self.assertGetSetCorrect(self.frame, 'from_address', 0, 333333)
        self.assertGetSetCorrect(self.frame, 'from_function_name', None, 'teh_caller')
        self.assertGetSetCorrect(self.frame, 'from_function_offset', 0, 0x31337)
        self.assertGetSetCorrect(self.frame, 'from_function_length', 0, 1024)
        self.assertGetSetCorrect(self.frame, 'from_module_name', None, 'asdf')

    def test_hash(self):
        self.assertHashable(self.frame)

if __name__ == '__main__':
    unittest.main()
