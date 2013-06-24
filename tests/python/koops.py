#!/usr/bin/env python

import unittest
from test_helpers import BindingsTestCase, load_input_contents

try:
    import _satyr as satyr
except ImportError:
    import satyr

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

        true_flags = { f for (f, v) in tainted_koops.taint_flags.items() if v == True}
        self.assertEqual(true_flags,
            set(['module_proprietary', 'page_release', 'died_recently', 'module_out_of_tree']))

    def test_dup(self):
        dup = self.koops.dup()
        self.assertNotEqual(id(dup.frames), id(self.koops.frames))
        self.assertEqual(dup.frames, self.koops.frames)

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
        self.assertEqual(dup, dup)
        self.assertEqual(dup, self.frame)
        self.assertEqual(dup, self.frame)
        self.assertNotEqual(id(dup), id(self.frame))

        dup.reliable = False
        self.assertNotEqual(dup, self.frame)
        self.assertTrue(dup > self.frame)
        self.assertFalse(dup < self.frame)

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

if __name__ == '__main__':
    unittest.main()
