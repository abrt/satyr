#!/usr/bin/env python

import unittest
from test_helpers import *

contents = load_input_contents('../java_stacktraces/java-02')
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

class TestJavaStacktrace(BindingsTestCase):
    def setUp(self):
        self.trace = satyr.JavaStacktrace(contents)

    def test_correct_thread_count(self):
        self.assertEqual(len(self.trace.threads), threads_expected)

    def test_correct_frame_count(self):
        self.assertEqual(frame_count(self.trace), frames_expected)

    def test_dup(self):
        dup = self.trace.dup()
        self.assertNotEqual(id(dup.threads), id(self.trace.threads))
        self.assertTrue(all(map(lambda t1, t2: t1.equals(t2), dup.threads, self.trace.threads)))

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

    def test_bthash(self):
        self.assertEqual(self.trace.get_bthash(), '184370433901aa9f14bfc85c29576e24ebca2c2e')

    def test_from_json(self):
        trace = satyr.JavaStacktrace.from_json('{}')
        self.assertEqual(trace.threads, [])

        json_text = '''
        {
            "threads": [
                {
                    "name": "ProbablySomeThreadOrSomethingImpl",
                    "frames": [
                        {
                            "name": "julia",
                            "file_name": "/u/b/f",
                            "file_line": 9000,
                            "class_path": "i don't know",
                            "is_native": false,
                            "is_exception": false,
                            "message": "Hi!"
                        },
                        {},
                        {
                            "name": "helen",
                            "is_exception": true
                        }
                    ],
                    "what is": "this",
                },
                {
                    "name": "TheOtherThread",
                    "frames": [
                        {
                            "name": "tiffany",
                            "file_name": "blahz",
                            "is_native": true,
                        }
                    ]
                }
            ]
        }
'''
        trace = satyr.JavaStacktrace.from_json(json_text)
        self.assertEqual(len(trace.threads), 2)
        self.assertEqual(len(trace.threads[0].frames), 3)
        self.assertEqual(len(trace.threads[1].frames), 1)

        self.assertEqual(trace.threads[0].name, 'ProbablySomeThreadOrSomethingImpl')
        self.assertEqual(trace.threads[1].name, 'TheOtherThread')

        self.assertEqual(trace.threads[0].frames[0].name, 'julia')
        self.assertEqual(trace.threads[0].frames[0].file_name, '/u/b/f')
        self.assertEqual(trace.threads[0].frames[0].file_line, 9000)
        self.assertEqual(trace.threads[0].frames[0].class_path, 'i don\'t know')
        self.assertFalse(trace.threads[0].frames[0].is_native)
        self.assertFalse(trace.threads[0].frames[0].is_exception)
        self.assertEqual(trace.threads[0].frames[0].message, 'Hi!')

        self.assertEqual(trace.threads[0].frames[1].name, None)
        self.assertEqual(trace.threads[0].frames[1].file_name, None)
        self.assertEqual(trace.threads[0].frames[1].file_line, 0)
        self.assertEqual(trace.threads[0].frames[1].class_path, None)
        self.assertFalse(trace.threads[0].frames[1].is_native)
        self.assertFalse(trace.threads[0].frames[1].is_exception)
        self.assertEqual(trace.threads[0].frames[1].message, None)

        self.assertEqual(trace.threads[0].frames[2].name, 'helen')
        self.assertEqual(trace.threads[0].frames[2].file_name, None)
        self.assertEqual(trace.threads[0].frames[2].file_line, 0)
        self.assertEqual(trace.threads[0].frames[2].class_path, None)
        self.assertFalse(trace.threads[0].frames[2].is_native)
        self.assertTrue(trace.threads[0].frames[2].is_exception)
        self.assertEqual(trace.threads[0].frames[2].message, None)

        self.assertEqual(trace.threads[1].frames[0].name, 'tiffany')
        self.assertEqual(trace.threads[1].frames[0].file_name, 'blahz')
        self.assertEqual(trace.threads[1].frames[0].file_line, 0)
        self.assertEqual(trace.threads[1].frames[0].class_path, None)
        self.assertTrue(trace.threads[1].frames[0].is_native)
        self.assertFalse(trace.threads[1].frames[0].is_exception)
        self.assertEqual(trace.threads[1].frames[0].message, None)

    def test_hash(self):
        self.assertHashable(self.trace)

class TestJavaThread(BindingsTestCase):
    def setUp(self):
        self.thread = satyr.JavaStacktrace(contents).threads[0]

    def test_getset(self):
        self.assertGetSetCorrect(self.thread, 'name', None, 'elvis')

    def test_cmp(self):
        self.assertTrue(self.thread.equals(self.thread))
        dup = self.thread.dup()
        self.assertTrue(self.thread.equals(dup))
        dup.name = ' 45678987\n\n\n\n'
        self.assertFalse(self.thread.equals(dup))

    def test_duphash(self):
        expected_plain = '''Thread
org.hibernate.exception.ConstraintViolationException
org.hibernate.exception.SQLStateConverter.convert
org.hibernate.exception.JDBCExceptionHelper.convert
'''
        self.assertEqual(self.thread.get_duphash(flags=satyr.DUPHASH_NOHASH, frames=3), expected_plain)
        self.assertEqual(self.thread.get_duphash(), '81450a80a9d9307624b08e80dc244beb63d91138')

    def test_hash(self):
        self.assertHashable(self.thread)

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
        self.assertTrue(dup.equals(dup))
        self.assertTrue(dup.equals(self.frame))
        self.assertNotEqual(id(dup), id(self.frame))
        dup.name = 'another'
        self.assertFalse(dup.equals(self.frame))

    def test_getset(self):
        self.assertGetSetCorrect(self.frame, 'name', 'org.hibernate.exception.ConstraintViolationException', 'long.name.is.Long')
        self.assertGetSetCorrect(self.frame, 'file_name', None, 'java.java')
        self.assertGetSetCorrect(self.frame, 'file_line', 0, 1234)
        self.assertGetSetCorrect(self.frame, 'class_path', None, '/path/to/class')
        self.assertGetSetCorrect(self.frame, 'is_exception', True, False)
        self.assertGetSetCorrect(self.frame, 'is_exception', False, 1)
        self.assertGetSetCorrect(self.frame, 'is_native', False, True)
        self.assertGetSetCorrect(self.frame, 'message', 'could not insert: [com.example.myproject.MyEntity]', 'printer on fire')

    def test_hash(self):
        self.assertHashable(self.frame)


if __name__ == '__main__':
    unittest.main()
