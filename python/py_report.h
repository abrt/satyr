/*
    py_report.h

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
#ifndef SATYR_PY_REPORT_H
#define SATYR_PY_REPORT_H

/**
 * @file
 * @brief Python bindings for struct sr_report.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_report_type;

struct sr_py_report
{
    PyObject_HEAD
    struct sr_report *report;
    PyObject *operating_system;
    PyObject *packages;
    PyObject *stacktrace;
};

/**
 * Constructor.
 */
PyObject *sr_py_report_new(PyTypeObject *object,
                           PyObject *args, PyObject *kwds);

/**
 * Destructor.
 */
void sr_py_report_free(PyObject *object);

/**
 * str
 */
PyObject *sr_py_report_str(PyObject *self);

/**
 * getters & setters
 */
PyObject *sr_py_report_get_version(PyObject *self, void *data);
PyObject *sr_py_report_get_type(PyObject *self, void *data);
int sr_py_report_set_type(PyObject *self, PyObject *rhs, void *data);
PyObject *sr_py_report_get_auth(PyObject *self, void *data);
int sr_py_report_set_auth(PyObject *self, PyObject *rhs, void *data);

/**
 * Methods.
 */
PyObject *
sr_py_report_to_json(PyObject *self, PyObject *args);

#ifdef __cplusplus
}
#endif

#endif
