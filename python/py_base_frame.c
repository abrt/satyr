/*
    py_base_frame.c

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
#include "py_base_frame.h"

#include <inttypes.h>

#include "frame.h"
#include "strbuf.h"

#define frame_doc "satyr.BaseFrame - base class for call frames"

#define f_short_string_doc "Usage: frame.short_string()\n\n" \
                           "Returns: string - brief textual representation of the frame"
#define f_equals_doc "Usage: frame.equals(otherframe)\n\n" \
                     "Returns: bool - True if frame has attributes equal to otherframe"

static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "short_string", sr_py_base_frame_short_string, METH_NOARGS,  f_short_string_doc },
    { "equals",       sr_py_base_frame_equals,       METH_VARARGS, f_equals_doc },
    { NULL },
};

PyTypeObject
sr_py_base_frame_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.BaseFrame",          /* tp_name */
    sizeof(struct sr_py_base_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    NULL,                       /* tp_dealloc */
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
    NULL,                       /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    frame_doc,                  /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    frame_methods,              /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */ //TODO throw an exception?
    NULL,                       /* tp_alloc */
    NULL,                       /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* methods */
PyObject *
sr_py_base_frame_short_string(PyObject *self, PyObject *args)
{
    struct sr_frame *frame = ((struct sr_py_base_frame*)self)->frame;
    struct sr_strbuf *strbuf = sr_strbuf_new();

    sr_frame_append_to_str(frame, strbuf);
    char *str = sr_strbuf_free_nobuf(strbuf);

    PyObject *result = PyString_FromString(str);
    free(str);
    return result;
}

static int
sr_py_base_frame_cmp(struct sr_py_base_frame *self, struct sr_py_base_frame *other)
{
    if (Py_TYPE(self) != Py_TYPE(other))
    {
        /* distinct types must be unequal */
        return normalize_cmp(Py_TYPE(self) - Py_TYPE(other));
    }

    return normalize_cmp(sr_frame_cmp(self->frame, other->frame));
}

PyObject *
sr_py_base_frame_equals(PyObject *self, PyObject *args)
{
    PyObject *other;

    if (!PyArg_ParseTuple(args, "O!", &sr_py_base_frame_type, &other))
        return NULL;

    if (sr_py_base_frame_cmp((struct sr_py_base_frame *)self,
                             (struct sr_py_base_frame *)other) == 0)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}
