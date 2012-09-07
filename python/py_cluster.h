/*
    py_cluster.h

    Copyright (C) 2012  Red Hat, Inc.

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
#ifndef BTPARSER_PY_CLUSTER_H
#define BTPARSER_PY_CLUSTER_H

/**
 * @file
 * @brief Python bindings for clustering.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject btp_py_dendrogram_type;

struct btp_py_dendrogram
{
    PyObject_HEAD
    struct btp_dendrogram *dendrogram;
};

/* constructor */
PyObject *btp_py_dendrogram_new(PyTypeObject *object,
                                PyObject *args,
                                PyObject *kwds);

/* destructor */
void btp_py_dendrogram_free(PyObject *object);

/* str */
PyObject *btp_py_dendrogram_str(PyObject *self);

/* getters & setters */
PyObject *btp_py_dendrogram_get_size(PyObject *self, PyObject *args);
PyObject *btp_py_dendrogram_get_object(PyObject *self, PyObject *args);
PyObject *btp_py_dendrogram_get_merge_level(PyObject *self, PyObject *args);

/* methods */
PyObject *btp_py_dendrogram_cut(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
