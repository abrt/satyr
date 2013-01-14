/*
    py_python_frame.h

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
#ifndef BTPARSER_PY_PYTHON_FRAME_H
#define BTPARSER_PY_PYTHON_FRAME_H

/**
 * @file
 * @brief Python bindings for PYTHON frame.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

PyTypeObject btp_py_python_frame_type;

struct btp_py_python_frame
{
    PyObject_HEAD
    struct btp_python_frame *frame;
};

/**
 * Constructor.
 */
PyObject *btp_py_python_frame_new(PyTypeObject *object,
                               PyObject *args, PyObject *kwds);

/**
 * Destructor.
 */
void btp_py_python_frame_free(PyObject *object);

/**
 * str
 */
PyObject *btp_py_python_frame_str(PyObject *self);

/* getters & setters */
PyObject *btp_py_python_frame_get_file_name(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_set_file_name(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_get_file_line(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_set_file_line(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_is_module(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_set_is_module(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_get_function_name(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_set_function_name(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_get_line_contents(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_set_line_contents(PyObject *self, PyObject *args);

/* methods */
PyObject *btp_py_python_frame_dup(PyObject *self, PyObject *args);
PyObject *btp_py_python_frame_cmp(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
