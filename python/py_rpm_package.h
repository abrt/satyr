/*
    py_rpm_package.h

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
#ifndef SATYR_PY_RPM_PACKAGE_H
#define SATYR_PY_RPM_PACKAGE_H

/**
 * @file
 * @brief Python bindings for RPM package.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

extern PyTypeObject sr_py_rpm_package_type;

struct sr_py_rpm_package
{
    PyObject_HEAD
    struct sr_rpm_package *rpm_package;
};

/**
 * Constructor.
 */
PyObject *sr_py_rpm_package_new(PyTypeObject *object,
                                PyObject *args, PyObject *kwds);

/**
 * Destructor.
 */
void sr_py_rpm_package_free(PyObject *object);

/**
 * str
 */
PyObject *sr_py_rpm_package_str(PyObject *self);

/* getters & setters */
PyObject *sr_py_rpm_package_get_role(PyObject *self, void *data);
int sr_py_rpm_package_set_role(PyObject *self, PyObject *rhs, void *data);

#ifdef __cplusplus
}
#endif

#endif
