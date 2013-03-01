/*
    py_python_stacktrace.h

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
#ifndef SATYR_PY_PYTHON_STACKTRACE_H
#define SATYR_PY_PYTHON_STACKTRACE_H

/**
 * @file
 * @brief Python bindings for kerneloops stack trace.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

PyTypeObject sr_py_python_stacktrace_type;

struct sr_py_python_stacktrace
{
    PyObject_HEAD
    PyObject *frames;
    struct sr_python_stacktrace *stacktrace;
};

/* constructor */
PyObject *sr_py_python_stacktrace_new(PyTypeObject *object,
                                      PyObject *args,
                                      PyObject *kwds);

/* destructor */
void sr_py_python_stacktrace_free(PyObject *object);

/* str */
PyObject *sr_py_python_stacktrace_str(PyObject *self);

/* getters & setters */
PyObject *sr_py_python_stacktrace_get_file_name(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_set_file_name(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_get_file_line(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_set_file_line(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_get_exception_name(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_set_exception_name(PyObject *self, PyObject *args);


/* methods */
PyObject *sr_py_python_stacktrace_dup(PyObject *self, PyObject *args);
PyObject *sr_py_python_stacktrace_normalize(PyObject *self, PyObject *args);


#ifdef __cplusplus
}
#endif

#endif
