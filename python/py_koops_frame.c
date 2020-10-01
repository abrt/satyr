/*
    py_koops_frame.c

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
#include "py_common.h"
#include "py_base_frame.h"
#include "py_koops_frame.h"

#include <inttypes.h>

#include "location.h"
#include "utils.h"
#include "koops/frame.h"

#define frame_doc "satyr.KerneloopsFrame - class representing a frame in a kerneloops\n\n" \
                  "Usage:\n\n" \
                  "satyr.KerneloopsFrame() - creates an empty frame\n\n" \
                  "satyr.KerneloopsFrame(str) - parses str and fills the frame object"

#define f_dup_doc "Usage: frame.dup()\n\n" \
                  "Returns: satyr.KerneloopsFrame - a new clone of frame\n\n" \
                  "Clones the frame object. All new structures are independent " \
                  "of the original object."


static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "dup",                       sr_py_koops_frame_dup,                      METH_NOARGS,  f_dup_doc                        },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_koops_frame
#define GSOFF_PY_MEMBER frame
#define GSOFF_C_STRUCT sr_koops_frame
GSOFF_START
GSOFF_MEMBER(address),
GSOFF_MEMBER(reliable),
GSOFF_MEMBER(function_name),
GSOFF_MEMBER(function_offset),
GSOFF_MEMBER(function_length),
GSOFF_MEMBER(module_name),
GSOFF_MEMBER(from_address),
GSOFF_MEMBER(from_function_name),
GSOFF_MEMBER(from_function_offset),
GSOFF_MEMBER(from_function_length),
GSOFF_MEMBER(from_module_name),
GSOFF_MEMBER(special_stack)
GSOFF_END

static PyGetSetDef
frame_getset[] =
{
    SR_ATTRIBUTE_BOOL  (reliable,             "True if the the frame is guaranteed to be real (bool)"),
    SR_ATTRIBUTE_UINT64(address,              "Address of the current instruction (long)"            ),
    SR_ATTRIBUTE_STRING(function_name,        "Function name (string)"                               ),
    SR_ATTRIBUTE_UINT64(function_offset,      "Function offset (long)"                               ),
    SR_ATTRIBUTE_UINT64(function_length,      "Function length (long)"                               ),
    SR_ATTRIBUTE_STRING(module_name,          "Module owning the function (string)"                  ),
    SR_ATTRIBUTE_UINT64(from_address,         "Address of the caller's instruction (long)"           ),
    SR_ATTRIBUTE_STRING(from_function_name,   "Caller function name (string)"                        ),
    SR_ATTRIBUTE_UINT64(from_function_offset, "Caller function offset (long)"                        ),
    SR_ATTRIBUTE_UINT64(from_function_length, "Caller function length (long)"                        ),
    SR_ATTRIBUTE_STRING(from_module_name,     "Module owning the caller function (string)"           ),
    SR_ATTRIBUTE_STRING(special_stack,        "Identifier of x86_64 kernel stack (string)"           ),
    { NULL },
};

PyTypeObject
sr_py_koops_frame_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.KerneloopsFrame",   /* tp_name */
    sizeof(struct sr_py_koops_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_koops_frame_free,     /* tp_dealloc */
    0,                          /* tp_vectorcall_offset */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    NULL,                       /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    sr_py_koops_frame_str,      /* tp_str */
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
    sr_py_koops_frame_new,      /* tp_new */
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
sr_py_koops_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_koops_frame *fo = (struct sr_py_koops_frame*)
        PyObject_New(struct sr_py_koops_frame, &sr_py_koops_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        fo->frame = sr_koops_frame_parse(&str);
    }
    else
        fo->frame = sr_koops_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
sr_py_koops_frame_free(PyObject *object)
{
    struct sr_py_koops_frame *this = (struct sr_py_koops_frame*)object;
    sr_koops_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_koops_frame_str(PyObject *self)
{
    struct sr_py_koops_frame *this = (struct sr_py_koops_frame*)self;
    GString *buf = g_string_new(NULL);
    if (this->frame->special_stack)
        g_string_append_printf(buf, "[%s] ", this->frame->special_stack);

    if (this->frame->address != 0)
        g_string_append_printf(buf, "[0x%016"PRIx64"] ", this->frame->address);

    if (!this->frame->reliable)
        g_string_append(buf, "? ");

    if (this->frame->function_name)
        g_string_append(buf, this->frame->function_name);

    if (this->frame->function_offset)
        g_string_append_printf(buf, "+0x%"PRIx64, this->frame->function_offset);

    if (this->frame->function_length)
        g_string_append_printf(buf, "/0x%"PRIx64, this->frame->function_length);

    if (this->frame->module_name)
        g_string_append_printf(buf, " [%s]", this->frame->module_name);

    if (this->frame->from_function_name || this->frame->from_address)
        g_string_append(buf, " from ");

    if (this->frame->from_address != 0)
        g_string_append_printf(buf, "[0x%016"PRIx64"] ", this->frame->from_address);

    if (this->frame->from_function_name)
        g_string_append(buf, this->frame->from_function_name);

    if (this->frame->from_function_offset)
        g_string_append_printf(buf, "+0x%"PRIx64, this->frame->from_function_offset);

    if (this->frame->from_function_length)
        g_string_append_printf(buf, "/0x%"PRIx64, this->frame->from_function_length);

    if (this->frame->from_module_name)
        g_string_append_printf(buf, " [%s]", this->frame->from_module_name);

    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_koops_frame_dup(PyObject *self, PyObject *args)
{
    struct sr_py_koops_frame *this = (struct sr_py_koops_frame*)self;
    struct sr_py_koops_frame *fo = (struct sr_py_koops_frame*)
        PyObject_New(struct sr_py_koops_frame, &sr_py_koops_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_koops_frame_dup(this->frame, false);

    return (PyObject*)fo;
}
