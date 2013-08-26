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

class TestRpmPackage(BindingsTestCase):
    def setUp(self):
        self.rpm = satyr.RpmPackage('emacs-filesystem', 1, '24.3', '11.fc19', 'noarch')

    def test_str(self):
        self.assertEqual(str(self.rpm), 'emacs-filesystem-1:24.3-11.fc19.noarch')

    def test_getset(self):
        self.assertGetSetCorrect(self.rpm, 'name', 'emacs-filesystem', 'msword')
        self.assertGetSetCorrect(self.rpm, 'epoch', 1, 0)
        self.assertGetSetCorrect(self.rpm, 'version', '24.3', '48')
        self.assertGetSetCorrect(self.rpm, 'release', '11.fc19', '1.asdf')
        self.assertGetSetCorrect(self.rpm, 'architecture', 'noarch', 'armv7hl')
        self.assertGetSetCorrect(self.rpm, 'install_time', 0, 1234565)
        self.assertGetSetCorrect(self.rpm, 'role', satyr.ROLE_UNKNOWN, satyr.ROLE_AFFECTED)


if __name__ == '__main__':
    unittest.main()

