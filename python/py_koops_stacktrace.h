/*
    py_koops_stacktrace.h

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
#ifndef SATYR_PY_KOOPS_STACKTRACE_H
#define SATYR_PY_KOOPS_STACKTRACE_H

/**
 * @file
 * @brief Python bindings for kerneloops stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_koops_stacktrace_type;

/* The beginning of this structure has to have the same layout as
 * sr_py_base_thread.
 */
struct sr_py_koops_stacktrace
{
    PyObject_HEAD
    struct sr_koops_stacktrace *stacktrace;
    PyObject *frames;
    PyTypeObject *frame_type;
    PyObject *taint_flags;
};

/* helpers */
PyObject *koops_stacktrace_to_python_obj(struct sr_koops_stacktrace *stacktrace);

/* constructor */
PyObject *sr_py_koops_stacktrace_new(PyTypeObject *object,
                                     PyObject *args,
                                     PyObject *kwds);

/* destructor */
void sr_py_koops_stacktrace_free(PyObject *object);

/* str */
PyObject *sr_py_koops_stacktrace_str(PyObject *self);

/* getters & setters */
PyObject *sr_py_koops_stacktrace_get_modules(PyObject *self, void *data);
int sr_py_koops_stacktrace_set_modules(PyObject *self, PyObject *rhs, void *data);
PyObject *sr_py_koops_stacktrace_get_taint_flags(PyObject *self, void *data);
int sr_py_koops_stacktrace_set_taint_flags(PyObject *self, PyObject *rhs, void *data);

/* methods */
PyObject *sr_py_koops_stacktrace_dup(PyObject *self, PyObject *args);
PyObject *sr_py_koops_stacktrace_normalize(PyObject *self, PyObject *args);


#ifdef __cplusplus
}
#endif

#endif
