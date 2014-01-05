import os
import sys
import unittest

# should import satyr.py which imports ../../python/.libs/_satyr.so
import satyr

class BindingsTestCase(unittest.TestCase):
    def assertGetSetCorrect(self, obj, attr, orig_val, new_val):
        '''
        Check whether getting/setting an attribute works correctly.
        '''
        self.assertEqual(obj.__getattribute__(attr), orig_val)
        obj.__setattr__(attr, new_val)
        self.assertEqual(obj.__getattribute__(attr), new_val)
        self.assertRaises(TypeError, obj.__delattr__, attr)

    def assertHashable(self, obj):
        '''
        Check whether obj is hashable.
        '''
        self.assertTrue(isinstance(hash(obj), int))

def load_input_contents(path):
    if not os.path.isfile(path):
        path = '../' + path

    with open(path, 'r') as f:
        return f.read()

def frame_count(trace):
    if hasattr(trace, 'frames'):
        return len(trace.frames)
    else:
        return sum(map(lambda x: len(x.frames), trace.threads))

if __name__ == '__main__':
    import sys
    sys.exit('This module is not meant to be run directly')
