/*
    py_common.h

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
#ifndef SATYR_PY_COMMON_H
#define SATYR_PY_COMMON_H

/**
 * @file
 * @brief Common routines for python bindings.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <structmember.h>

/*
 * Struct member access using offsets. The second variant is useful for nested
 * structures.
 *
 * MEMB_T(type, st, offsetof(typeof(st), member)) == (type)(st->member)
 * MEMB(st, offsetof(typeof(st), member)) == (void*)(st->member)
 */
#define MEMB_T(type, st, off) *(type *)((char*)st + off)
#define MEMB(st, off) MEMB_T(void*, st, off)

/*
 * This struct is used for generic getters and setters. Most of satyr's python
 * object structures contain pointer to a corresponding C structure, which in
 * turn contains the members that are exposed in the python API.
 *
 * If we know the offset of the pointer to the C structure in the python object
 * and the offset of the member in the C structure, we can access the member
 * using generic (per-type) functions, declared further below.
 */
struct getset_offsets
{
    size_t c_struct_offset;
    size_t member_offset;
};

/*
 * These macros make it possible to initialize several struct getset_offsets
 * without typing too much and entering the offsets by hand.
 *
 * You need to define GSOFF_PY_STRUCT to be the type of the structure defining
 * the python object, GSOFF_PY_MEMBER to be the name of its member pointing to
 * the C structure, and GSOFF_C_STRUCT to be the type of the C structure. Use
 * GSOFF_MEMBER for each member you want to access, GSOFF_MEMBER(m) defines the
 * variable gsoff_m which can be used in the declaration of getters and
 * setters.
 *
 * See python/py_gdb_frame.c for example usage.
 */
#define GSOFF_START static struct getset_offsets
#define GSOFF_MEMBER(c_member)                                                \
    gsoff_ ## c_member = {                                                    \
        .c_struct_offset = offsetof(struct GSOFF_PY_STRUCT, GSOFF_PY_MEMBER), \
        .member_offset = offsetof(struct GSOFF_C_STRUCT, c_member)            \
    }
#define GSOFF_END ;

/*
 * Once you declare GSOFF_MEMBER() macro for each member you want to expose in
 * python, you can use following macros to define PyGetSetDef structures.
 *
 * The _R variants stand for "rename" in case you want the python attribute to
 * have different than the corresponding C member.
 *
 * See python/py_gdb_sharedlib.c for sample usage of both.
 */
#define SR_ATTRIBUTE_STRING_R(name, c_member, doc_str) \
        { (char*)name, sr_py_getter_string, sr_py_setter_string, (char*)doc_str, &gsoff_ ## c_member }
#define SR_ATTRIBUTE_STRING(c_member, doc_str) SR_ATTRIBUTE_STRING_R(#c_member, c_member, doc_str)

#define SR_ATTRIBUTE_UINT16_R(name, c_member, doc_str) \
        { (char*)name, sr_py_getter_uint16, sr_py_setter_uint16, (char*)doc_str, &gsoff_ ## c_member }
#define SR_ATTRIBUTE_UINT16(c_member, doc_str) SR_ATTRIBUTE_UINT16_R(#c_member, c_member, doc_str)

#define SR_ATTRIBUTE_UINT32_R(name, c_member, doc_str) \
        { (char*)name, sr_py_getter_uint32, sr_py_setter_uint32, (char*)doc_str, &gsoff_ ## c_member }
#define SR_ATTRIBUTE_UINT32(c_member, doc_str) SR_ATTRIBUTE_UINT32_R(#c_member, c_member, doc_str)

#define SR_ATTRIBUTE_UINT64_R(name, c_member, doc_str) \
        { (char*)name, sr_py_getter_uint64, sr_py_setter_uint64, (char*)doc_str, &gsoff_ ## c_member }
#define SR_ATTRIBUTE_UINT64(c_member, doc_str) SR_ATTRIBUTE_UINT64_R(#c_member, c_member, doc_str)

#define SR_ATTRIBUTE_BOOL_R(name, c_member, doc_str) \
        { (char*)name, sr_py_getter_bool, sr_py_setter_bool, (char*)doc_str, &gsoff_ ## c_member }
#define SR_ATTRIBUTE_BOOL(c_member, doc_str) SR_ATTRIBUTE_BOOL_R(#c_member, c_member, doc_str)

/*
 * Generic getters/setters that accept a struct getset_offsets as the data
 * argument.
 */
PyObject *sr_py_getter_string(PyObject *self, void *data);
int sr_py_setter_string(PyObject *self, PyObject *rhs, void *data);

PyObject *sr_py_getter_uint16(PyObject *self, void *data);
int sr_py_setter_uint16(PyObject *self, PyObject *rhs, void *data);

PyObject *sr_py_getter_uint32(PyObject *self, void *data);
int sr_py_setter_uint32(PyObject *self, PyObject *rhs, void *data);

PyObject *sr_py_getter_bool(PyObject *self, void *data);
int sr_py_setter_bool(PyObject *self, PyObject *rhs, void *data);

PyObject *sr_py_getter_uint64(PyObject *self, void *data);
int sr_py_setter_uint64(PyObject *self, PyObject *rhs, void *data);

/*
 * Satyr's comparison functions return arbitrary numbers, while python
 * requires the result to be in {-1, 0, 1}.
 */
int normalize_cmp(int n);

#ifdef __cplusplus
}
#endif

#endif /* SATYR_PY_COMMON_H */
