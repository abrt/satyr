/*
    py_common.c

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

#include "py_common.h"

#include "utils.h"

/*
 * Generic getters/setters that accept a struct getset_offsets as the data
 * argument.
 */

PyObject *
sr_py_getter_string(PyObject *self, void *data)
{
    struct getset_offsets *gsoff = data;
    char *str = MEMB_T(char*, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);

    if (str == NULL)
        Py_RETURN_NONE;

    return PyString_FromString(str);
}

int
sr_py_setter_string(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    struct getset_offsets *gsoff = data;

    const char *newvalue = PyUnicode_AsUTF8(rhs);
    if (!newvalue)
        return -1;

    char *str = MEMB_T(char*, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);
    free(str);
    MEMB_T(char*, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset) = g_strdup(newvalue);

    return 0;
}

PyObject *
sr_py_getter_uint16(PyObject *self, void *data)
{
    struct getset_offsets *gsoff = data;
    uint16_t num = MEMB_T(uint16_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);
    return PyInt_FromLong(num);
}

int
sr_py_setter_uint16(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    struct getset_offsets *gsoff = data;

    long newvalue = PyInt_AsLong(rhs);
    if (PyErr_Occurred())
        return -1;

    if (newvalue < 0 || newvalue > UINT16_MAX)
    {
        PyErr_SetString(PyExc_ValueError, "Negative or too large value.");
        return -1;
    }

    MEMB_T(uint16_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset)
        = (uint16_t)newvalue;
    return 0;
}

PyObject *
sr_py_getter_uint32(PyObject *self, void *data)
{
    struct getset_offsets *gsoff = data;
    uint32_t num = MEMB_T(uint32_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);
    return PyInt_FromLong(num);
}

int
sr_py_setter_uint32(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    struct getset_offsets *gsoff = data;

    long newvalue = PyInt_AsLong(rhs);
    if (PyErr_Occurred())
        return -1;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Value must not be negative.");
        return -1;
    }

    MEMB_T(uint32_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset)
        = (uint32_t)newvalue;
    return 0;
}

PyObject *
sr_py_getter_bool(PyObject *self, void *data)
{
    struct getset_offsets *gsoff = data;

    bool b = MEMB_T(bool, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);
    return PyBool_FromLong(b);
}

int
sr_py_setter_bool(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    struct getset_offsets *gsoff = data;

    bool newvalue = PyObject_IsTrue(rhs);
    MEMB_T(bool, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset) = newvalue;

    return 0;
}

PyObject *
sr_py_getter_uint64(PyObject *self, void *data)
{
    struct getset_offsets *gsoff = data;

    uint64_t num
        = MEMB_T(uint64_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset);

    /* If the attribute is UINT64_MAX, return None. */
    if (num == (uint64_t) -1)
        Py_RETURN_NONE;

    return PyLong_FromUnsignedLongLong(num);
}

int
sr_py_setter_uint64(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    uint64_t newvalue;

    /* If rhs is None, set the attribute to UINT64_MAX. */
    if (rhs == Py_None)
    {
        newvalue = -1;
    }
    else
    {
        newvalue = (uint64_t)PyInt_AsUnsignedLongLongMask(rhs);
        if (PyErr_Occurred())
            return -1;
    }

    struct getset_offsets *gsoff = data;
    MEMB_T(uint64_t, MEMB(self, gsoff->c_struct_offset), gsoff->member_offset) = newvalue;
    return 0;
}

int
sr_py_setter_readonly(PyObject *self, PyObject *rhs, void *data)
{
    PyErr_SetString(PyExc_AttributeError, "This attribute is read-only.");
    return -1;
}

int
normalize_cmp(int n)
{
    if (n > 0)
        return 1;
    else if (n < 0)
        return -1;

    return 0;
}
