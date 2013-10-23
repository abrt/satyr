/*
    py_base_frame.h

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
#ifndef SATYR_PY_BASE_FRAME_H
#define SATYR_PY_BASE_FRAME_H

/**
 * @file
 * @brief Base class for call frames.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_base_frame_type;

struct sr_py_base_frame
{
    PyObject_HEAD
    struct sr_frame *frame;
};

PyObject *sr_py_base_frame_equals(PyObject *self, PyObject *args);

PyObject *sr_py_base_frame_short_string(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
