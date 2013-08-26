#!/usr/bin/env python

import unittest
from test_helpers import *

class TestOperatingSystem(BindingsTestCase):
    def setUp(self):
        self.opsys = satyr.OperatingSystem('fedora', '19', 'x86_64')

    def test_str(self):
        self.assertEqual(str(self.opsys), 'fedora 19 (x86_64)')

    def test_getset(self):
        self.assertGetSetCorrect(self.opsys, 'name', 'fedora', 'debian')
        self.assertGetSetCorrect(self.opsys, 'version', '19', 'XP')
        self.assertGetSetCorrect(self.opsys, 'architecture', 'x86_64', 's390x')
        self.assertGetSetCorrect(self.opsys, 'uptime', 0, 9000)
        self.assertGetSetCorrect(self.opsys, 'cpe', None, 'cpe:/o:fedoraproject:fedora:19')


if __name__ == '__main__':
    unittest.main()

