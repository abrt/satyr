#!/usr/bin/env python

import unittest
from test_helpers import *

class TestMisc(BindingsTestCase):
    def test_demangle(self):
        self.assertEqual(satyr.demangle_symbol('f'), 'f')
        self.assertEqual(satyr.demangle_symbol('_ZN9wikipedia7article8print_toERSo'),
                         'wikipedia::article::print_to(std::ostream&)')
        self.assertEqual(satyr.demangle_symbol('_ZN9wikipedia7article6formatEv'),
                         'wikipedia::article::format()')


if __name__ == '__main__':
    unittest.main()
