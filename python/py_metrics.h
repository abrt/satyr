/*
    py_metrics.h

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
#ifndef SATYR_PY_METRICS_H
#define SATYR_PY_METRICS_H

/**
 * @file
 * @brief Python bindings for metrics.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_distances_type;
extern PyTypeObject sr_py_distances_part_type;

struct sr_py_distances
{
    PyObject_HEAD
    struct sr_distances *distances;
};

/* constructor */
PyObject *sr_py_distances_new(PyTypeObject *object,
                              PyObject *args,
                              PyObject *kwds);

/* destructor */
void sr_py_distances_free(PyObject *object);

/* str */
PyObject *sr_py_distances_str(PyObject *self);

/* getters & setters */
PyObject *sr_py_distances_get_size(PyObject *self, PyObject *args);
PyObject *sr_py_distances_get_distance(PyObject *self, PyObject *args);
PyObject *sr_py_distances_set_distance(PyObject *self, PyObject *args);

/* methods */
PyObject *sr_py_distances_dup(PyObject *self, PyObject *args);
PyObject *sr_py_distances_merge_parts(PyObject *self, PyObject *args);

struct sr_py_distances_part
{
    PyObject_HEAD
    struct sr_distances_part *distances_part;
};

/* constructor */
PyObject *sr_py_distances_part_new(PyTypeObject *object,
                                   PyObject *args,
                                   PyObject *kwds);

/* destructor */
void sr_py_distances_part_free(PyObject *object);

/* str */
PyObject *sr_py_distances_part_str(PyObject *self);

/* methods */
PyObject *sr_py_distances_part_create(PyObject *self, PyObject *args, PyObject *kwds);
PyObject *sr_py_distances_part_compute(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
