/*
    py_koops_frame.h

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
#ifndef SATYR_PY_KOOPS_FRAME_H
#define SATYR_PY_KOOPS_FRAME_H

/**
 * @file
 * @brief Python bindings for koops frame.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

PyTypeObject sr_py_koops_frame_type;

struct sr_py_koops_frame
{
    PyObject_HEAD
    struct sr_koops_frame *frame;
};

/**
 * Constructor.
 */
PyObject *sr_py_koops_frame_new(PyTypeObject *object,
                                PyObject *args, PyObject *kwds);

/**
 * Destructor.
 */
void sr_py_koops_frame_free(PyObject *object);

/**
 * str
 */
PyObject *sr_py_koops_frame_str(PyObject *self);

/* getters & setters */
PyObject *sr_py_koops_frame_get_reliable(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_reliable(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_address(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_address(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_function_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_function_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_function_offset(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_function_offset(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_function_length(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_function_length(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_module_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_module_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_from_address(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_from_address(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_from_function_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_from_function_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_from_function_offset(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_from_function_offset(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_from_function_length(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_from_function_length(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_get_from_module_name(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_set_from_module_name(PyObject *self, PyObject *args);


/* methods */
PyObject *sr_py_koops_frame_dup(PyObject *self, PyObject *args);
PyObject *sr_py_koops_frame_cmp(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
