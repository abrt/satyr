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

    def test_hash(self):
        self.assertHashable(self.opsys)

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

    def test_hash(self):
        self.assertHashable(self.rpm)

class TestReport(BindingsTestCase):
    def setUp(self):
        self.report_json = load_input_contents('../json_files/ureport-1')
        self.report = satyr.Report(self.report_json)

    def test_to_json(self):
        '''
        Check that loading a report and converting it to json again produces
        the same output.
        '''
        import json

        source = json.loads(self.report_json)
        output = json.loads(self.report.to_json())
        self.assertEqual(source, output)

    def test_getset(self):
        self.assertGetSetCorrect(self.report, 'reporter_name', 'satyr', 'abrt')
        self.assertGetSetCorrect(self.report, 'reporter_version', '0.3', '0.2')
        self.assertGetSetCorrect(self.report, 'user_root', False, True)
        self.assertGetSetCorrect(self.report, 'user_local', True, False)
        self.assertGetSetCorrect(self.report, 'component_name', 'coreutils', 'zsh')
        self.assertGetSetCorrect(self.report, 'serial', 1, 44)

        self.assertGetSetCorrect(self.report, 'report_type', 'core', 'python')
        self.assertRaises(ValueError, self.report.__setattr__, 'report_type', 'C#')

        self.assertEqual(self.report.report_version, 2)
        self.assertRaises(AttributeError, self.report.__setattr__, 'report_version', 42)

    def test_attributes(self):
        self.assertTrue(self.report.stacktrace != None)
        self.assertTrue(isinstance(self.report.stacktrace, satyr.CoreStacktrace))
        self.assertTrue(len(self.report.stacktrace.threads) == 1)
        self.assertTrue(len(self.report.stacktrace.threads[0].frames) == 5)

        self.assertTrue(self.report.operating_system != None)
        self.assertTrue(isinstance(self.report.operating_system, satyr.OperatingSystem))
        self.assertEqual(self.report.operating_system.name, 'fedora')
        self.assertEqual(self.report.operating_system.version, '18')
        self.assertEqual(self.report.operating_system.architecture, 'x86_64')
        self.assertEqual(self.report.operating_system.cpe, None)

        self.assertTrue(self.report.packages != None)
        self.assertTrue(isinstance(self.report.packages, list))
        self.assertTrue(isinstance(self.report.packages[0], satyr.RpmPackage))
        self.assertTrue(len(self.report.packages) == 3)
        self.assertEqual(self.report.packages[0].name, 'coreutils')
        self.assertEqual(self.report.packages[0].role, satyr.ROLE_AFFECTED)

    def test_hash(self):
        self.assertHashable(self.report)

    def test_auth(self):
        self.assertFalse(self.report.auth)
        self.assertRaises(NotImplementedError, self.report.__setattr__, 'auth', {'hostname': 'darkstar'})

        report_with_auth = satyr.Report(load_input_contents('../json_files/ureport-1-auth'))
        self.assertEqual(report_with_auth.auth, {'hostname': 'localhost', 'machine_id': '0000'})

if __name__ == '__main__':
    unittest.main()

