#!/usr/bin/env python

import os
import unittest
from test_helpers import BindingsTestCase

try:
    import _satyr as satyr
except ImportError:
    import satyr

path = '../java_stacktraces/java-02'
threads_expected = 1
frames_expected = 32
expected_short_text = '''#1 org.hibernate.exception.ConstraintViolationException: could not insert: [com.example.myproject.MyEntity]
#2 \tat org.hibernate.exception.SQLStateConverter.convert(SQLStateConverter.java:96) [unknown]
#3 \tat org.hibernate.exception.JDBCExceptionHelper.convert(JDBCExceptionHelper.java:66) [unknown]
#4 \tat org.hibernate.id.insert.AbstractSelectingDelegate.performInsert(AbstractSelectingDelegate.java:64) [unknown]
#5 com.example.myproject.MyProjectServletException
#6 \tat com.example.myproject.MyServlet.doPost(MyServlet.java:169) [unknown]
#7 \tat javax.servlet.http.HttpServlet.service(HttpServlet.java:727) [unknown]
#8 \tat javax.servlet.http.HttpServlet.service(HttpServlet.java:820) [unknown]
'''

if not os.path.isfile(path):
    path = '../' + path

with file(path) as f:
    contents = f.read()

class TestJavaStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.JavaStacktrace(contents)

    def frame_count(self, trace):
        return sum(map(lambda x: len(x.frames), trace.threads))

    def test_correct_thread_count(self):
        self.assertEqual(len(self.trace.threads), threads_expected)

    def test_correct_frame_count(self):
        self.assertEqual(self.frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(id(dup.threads), id(self.trace.threads))
        self.assertEqual(dup.threads, self.trace.threads)

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
        self.assertTrue(('Java stacktrace with %d threads' % threads_expected) in out)

    def test_to_short_text(self):
        self.assertEqual(self.trace.to_short_text(8), expected_short_text)


class TestJavaThread(BindingsTestCase):
    def setUp(self):
        self.thread = satyr.JavaStacktrace(contents).threads[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.thread, 'name', None, 'elvis')

    def test_cmp(self):
        self.assertEqual(self.thread, self.thread)
        dup = self.thread.dup()
        self.assertEqual(self.thread, dup)
        dup.name = ' 45678987\n\n\n\n'
        self.assertNotEqual(self.thread, dup)

class TestJavaSharedlib(BindingsTestCase):
    def setUp(self):
        self.shlib = satyr.JavaStacktrace(contents).libs[0]

class TestJavaFrame(BindingsTestCase):
    def setUp(self):
        self.frame = satyr.JavaStacktrace(contents).threads[0].frames[0]

    def test_str(self):
        out = str(self.frame)
        self.assertTrue('org.hibernate.exception.ConstraintViolationException' in out)
        self.assertTrue('could not insert' in out)
        self.assertTrue('com.example.myproject' in out)

    def test_dup(self):
        dup = self.frame.dup()
        self.assertEqual(dup.name,
            self.frame.name)

        dup.name = 'other'
        self.assertNotEqual(dup.name,
            self.frame.name)

    def test_cmp(self):
        dup = self.frame.dup()
        self.assertEqual(dup, dup)
        self.assertEqual(dup, self.frame)
        self.assertEqual(dup, self.frame)
        self.assertNotEqual(id(dup), id(self.frame))
        dup.name = 'another'
        self.assertNotEqual(dup, self.frame)
        self.assertFalse(dup > self.frame)
        self.assertTrue(dup < self.frame)

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'name', 'org.hibernate.exception.ConstraintViolationException', 'long.name.is.Long')
        self.assertGetSetCorrect(self.frame, 'file_name', None, 'java.java')
        self.assertGetSetCorrect(self.frame, 'file_line', 0, 1234)
        self.assertGetSetCorrect(self.frame, 'class_path', None, '/path/to/class')
        self.assertGetSetCorrect(self.frame, 'is_exception', True, False)
        self.assertGetSetCorrect(self.frame, 'is_exception', False, 1)
        self.assertGetSetCorrect(self.frame, 'is_native', False, True)
        self.assertGetSetCorrect(self.frame, 'message', 'could not insert: [com.example.myproject.MyEntity]', 'printer on fire')


if __name__ == '__main__':
    unittest.main()
