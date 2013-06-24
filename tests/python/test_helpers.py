import os
import unittest

class BindingsTestCase(unittest.TestCase):
    def assertGetSetCorrect(self, obj, attr, orig_val, new_val):
        '''
        Check whether getting/setting an attribute works correctly.
        '''
        self.assertEqual(obj.__getattribute__(attr), orig_val)
        obj.__setattr__(attr, new_val)
        self.assertEqual(obj.__getattribute__(attr), new_val)
        self.assertRaises(TypeError, obj.__delattr__, attr)

def load_input_contents(path):
    if not os.path.isfile(path):
        path = '../' + path

    with file(path) as f:
        return f.read()

if __name__ == '__main__':
    import sys
    sys.exit('This module is not meant to be run directly')
