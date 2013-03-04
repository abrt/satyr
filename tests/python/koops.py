#!/usr/bin/env python

import os
import unittest

try:
    import _satyr as satyr
except ImportError:
    import satyr

path = '../kerneloopses/rhbz-827868'
frames_expected = 32
mods_expected = 91

if not os.path.isfile(path):
    path = '../' + path

with file(path) as f:
    contents = f.read()

class TestKerneloops(unittest.TestCase):
    def setUp(self):
        self.koops = satyr.Kerneloops(contents)

    def test_correct_frame_count(self):
        self.assertEqual(len(self.koops.frames), frames_expected)

    def test_correct_modules(self):
        self.assertEqual(len(self.koops.get_modules()), mods_expected)

    def test_dup(self):
        dup = self.koops.dup()
        self.assertNotEqual(dup.frames, self.koops.frames)

        dup.frames = dup.frames[:5]
        dup2 = dup.dup()
        self.assertEqual(len(dup.frames), len(dup2.frames))

    def test_normalize(self):
        dup = self.koops.dup()
        dup.normalize()
        self.assertNotEqual(len(dup.frames), len(self.koops.frames))
        self.assertNotEqual(dup.frames, self.koops.frames)

    def test_str(self):
        out = str(self.koops)
        self.assertIn('Kerneloops with %d frames' % frames_expected, out)

class TestKoopsFrame(unittest.TestCase):
    def setUp(self):
        self.frame = satyr.Kerneloops(contents).frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertIn('0xffffffff81057adf', out)
        self.assertIn('warn_slowpath_common', out)
        self.assertIn('0x7f', out)
        self.assertIn('0xc0', out)

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
        dup.set_reliable(False)
        self.assertNotEqual(dup.cmp(self.frame), 0)

if __name__ == '__main__':
    unittest.main()