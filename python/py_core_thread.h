/*
    py_core_thread.h

    Copyright (C) 2013  Red Hat, Inc.

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
#ifndef SATYR_PY_CORE_THREAD_H
#define SATYR_PY_CORE_THREAD_H

/**
 * @file
 * @brief Python bindings for Core thread.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_core_thread_type;

/* The beginning of this structure has to have the same layout as
 * sr_py_base_thread.
 */
struct sr_py_core_thread
{
    PyObject_HEAD
    struct sr_core_thread *thread;
    PyObject *frames;
    PyTypeObject *frame_type;
};

/* constructor */
PyObject *sr_py_core_thread_new(PyTypeObject *object,
                                PyObject *args,
                                PyObject *kwds);

/* destructor */
void sr_py_core_thread_free(PyObject *object);

/* str */
PyObject *sr_py_core_thread_str(PyObject *self);

/* methods */
PyObject *sr_py_core_thread_dup(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
