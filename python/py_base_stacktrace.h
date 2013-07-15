/*
    py_base_stacktrace.h

    Copyright (C) 2013 Red Hat, Inc.

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
#ifndef SATYR_PY_BASE_STACKTRACE_H
#define SATYR_PY_BASE_STACKTRACE_H

/**
 * @file
 * @brief Base classes for stacktrace structures.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

#include "py_base_thread.h"

extern PyTypeObject sr_py_single_stacktrace_type;
extern PyTypeObject sr_py_multi_stacktrace_type;

/* Python object for multi threaded stacktraces. */
struct sr_py_multi_stacktrace
{
    PyObject_HEAD
    struct sr_stacktrace *stacktrace;
    PyObject *threads;
    PyTypeObject *thread_type;
    PyTypeObject *frame_type;
};

/* helpers */
int threads_prepare_linked_list(struct sr_py_multi_stacktrace *stacktrace);
PyObject *threads_to_python_list(struct sr_stacktrace *stacktrace,
                                 PyTypeObject *thread_type, PyTypeObject *frame_type);

/* methods */
PyObject *sr_py_single_stacktrace_to_short_text(PyObject *self, PyObject *args);
PyObject *sr_py_multi_stacktrace_to_short_text(PyObject *self, PyObject *args);
PyObject *sr_py_single_stacktrace_get_bthash(PyObject *self, PyObject *args);
PyObject *sr_py_multi_stacktrace_get_bthash(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
