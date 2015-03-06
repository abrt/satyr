/*
    py_core_frame.c

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
#include "py_common.h"
#include "py_base_frame.h"
#include "py_core_frame.h"

#include "strbuf.h"
#include "utils.h"
#include "core/frame.h"

#define frame_doc "satyr.CoreFrame - class representing a frame in a native executable\n\n" \
                  "Usage: satyr.CoreFrame() - creates an empty frame"

#define f_dup_doc "Usage: frame.dup()\n\n" \
                  "Returns: satyr.CoreFrame - a new clone of the frame\n\n" \
                  "Clones the frame object. All new structures are independent " \
                  "of the original object."


static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "dup", sr_py_core_frame_dup, METH_NOARGS,  f_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_core_frame
#define GSOFF_PY_MEMBER frame
#define GSOFF_C_STRUCT sr_core_frame
GSOFF_START
GSOFF_MEMBER(address),
GSOFF_MEMBER(build_id),
GSOFF_MEMBER(build_id_offset),
GSOFF_MEMBER(function_name),
GSOFF_MEMBER(file_name),
GSOFF_MEMBER(fingerprint),
GSOFF_MEMBER(fingerprint_hashed)
GSOFF_END

static PyGetSetDef
frame_getset[] =
{
    SR_ATTRIBUTE_UINT64(address,            "Address of the machine code in memory (long)"      ),
    SR_ATTRIBUTE_STRING(build_id,           "Build ID of the ELF file (string)"                 ),
    SR_ATTRIBUTE_UINT64(build_id_offset,    "Offset of the instruction pointer from the start " \
                                            "of the executable segment (long)"                  ),
    SR_ATTRIBUTE_STRING(function_name,      "Function name (string)"                            ),
    SR_ATTRIBUTE_STRING(file_name,          "Name of the executable or shared library (string)" ),
    SR_ATTRIBUTE_STRING(fingerprint,        "Fingerprint of the current function (string)"      ),
    SR_ATTRIBUTE_BOOL  (fingerprint_hashed, "True if fingerprint is already hashed (bool)"      ),
    { NULL }
};

PyTypeObject
sr_py_core_frame_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.CoreFrame",          /* tp_name */
    sizeof(struct sr_py_core_frame), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_core_frame_free,      /* tp_dealloc */
    NULL,                       /* tp_print */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    NULL,                       /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    sr_py_core_frame_str,       /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    frame_doc,                  /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    frame_methods,              /* tp_methods */
    NULL,                       /* tp_members */
    frame_getset,               /* tp_getset */
    &sr_py_base_frame_type,     /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_core_frame_new,       /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* constructor */
PyObject *
sr_py_core_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_core_frame *fo = (struct sr_py_core_frame*)
        PyObject_New(struct sr_py_core_frame, &sr_py_core_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_core_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
sr_py_core_frame_free(PyObject *object)
{
    struct sr_py_core_frame *this = (struct sr_py_core_frame*)object;
    sr_core_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_core_frame_str(PyObject *self)
{
    struct sr_py_core_frame *this = (struct sr_py_core_frame*)self;
    struct sr_strbuf *buf = sr_strbuf_new();

    if (this->frame->address != 0)
        sr_strbuf_append_strf(buf, "[0x%016"PRIx64"] ", this->frame->address);

    if (this->frame->function_name)
        sr_strbuf_append_strf(buf, "%s ",  this->frame->function_name);

    if (this->frame->build_id)
        sr_strbuf_append_strf(buf, "%s+0x%"PRIx64" ", this->frame->build_id,
                              this->frame->build_id_offset);

    if (this->frame->file_name)
        sr_strbuf_append_strf(buf, "[%s] ", this->frame->file_name);

    if (this->frame->fingerprint)
        sr_strbuf_append_strf(buf, "fingerprint: %s (%shashed)", this->frame->fingerprint,
                              (this->frame->fingerprint_hashed ? "" : "not "));

    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_core_frame_dup(PyObject *self, PyObject *args)
{
    struct sr_py_core_frame *this = (struct sr_py_core_frame*)self;
    struct sr_py_core_frame *fo = (struct sr_py_core_frame*)
        PyObject_New(struct sr_py_core_frame, &sr_py_core_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_core_frame_dup(this->frame, false);

    return (PyObject*)fo;
}
