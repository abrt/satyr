/*
    py_java_stacktrace.h

    Copyright (C) 2010, 2011, 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#ifndef BTPARSER_PY_JAVA_STACKTRACE_H
#define BTPARSER_PY_JAVA_STACKTRACE_H

/**
 * @file
 * @brief Python bindings for Java stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

struct btp_py_java_frame;
struct btp_py_java_thread;

PyTypeObject btp_py_java_stacktrace_type;

struct btp_py_java_stacktrace
{
    PyObject_HEAD
    struct btp_java_stacktrace *stacktrace;
    PyObject *threads;
};

/* helpers */
int java_stacktrace_prepare_linked_list(struct btp_py_java_stacktrace *stacktrace);
PyObject *java_stacktrace_prepare_thread_list(struct btp_java_stacktrace *stacktrace);

/* constructor */
PyObject *btp_py_java_stacktrace_new(PyTypeObject *object,
                                    PyObject *args,
                                    PyObject *kwds);

/* destructor */
void btp_py_java_stacktrace_free(PyObject *object);

/* str */
PyObject *btp_py_java_stacktrace_str(PyObject *self);

/* methods */
PyObject *btp_py_java_stacktrace_dup(PyObject *self, PyObject *args);
PyObject *btp_py_java_stacktrace_normalize(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
