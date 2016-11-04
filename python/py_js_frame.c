/*
    py_js_frame.c

    Copyright (C) 2016  Red Hat, Inc.

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
#include "py_js_frame.h"
#include "location.h"
#include "strbuf.h"
#include "utils.h"
#include "js/frame.h"

#define frame_doc "satyr.JavaScriptFrame - class representing a JavaScript frame\n\n" \
                  "Usage:\n\n" \
                  "satyr.JavaScriptFrame() - creates an empty frame\n\n" \
                  "satyr.JavaScriptFrame(str) - parses str and fills the frame object"

#define f_dup_doc "Usage: frame.dup()\n\n" \
                  "Returns: satyr.JavaScriptFrame - a new clone of frame\n\n" \
                  "Clones the frame object. All new structures are independent " \
                  "of the original object."


static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "dup", sr_py_js_frame_dup, METH_NOARGS,  f_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_js_frame
#define GSOFF_PY_MEMBER frame
#define GSOFF_C_STRUCT sr_js_frame
GSOFF_START
GSOFF_MEMBER(file_name),
GSOFF_MEMBER(file_line),
GSOFF_MEMBER(line_column),
GSOFF_MEMBER(function_name)
GSOFF_END

static PyGetSetDef
frame_getset[] =
{
    SR_ATTRIBUTE_STRING(file_name,        "Source file name (string)"                              ),
    SR_ATTRIBUTE_UINT32(file_line,        "Source line number (positive integer)"                  ),
    SR_ATTRIBUTE_UINT32(line_column,      "Column of source line (integer)"                        ),
    SR_ATTRIBUTE_STRING(function_name,    "Function name (string)"                                 ),
    { NULL },
};

PyTypeObject
sr_py_js_frame_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.JavaScriptFrame",   /* tp_name */
    sizeof(struct sr_py_js_frame), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_js_frame_free,        /* tp_dealloc */
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
    sr_py_js_frame_str,         /* tp_str */
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
    sr_py_js_frame_new,         /* tp_new */
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
sr_py_js_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_js_frame *fo = (struct sr_py_js_frame*)
        PyObject_New(struct sr_py_js_frame, &sr_py_js_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        fo->frame = sr_js_frame_parse(&str, &location);

        if (!fo->frame)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        fo->frame = sr_js_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
sr_py_js_frame_free(PyObject *object)
{
    struct sr_py_js_frame *this = (struct sr_py_js_frame*)object;
    sr_js_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_js_frame_str(PyObject *self)
{
    struct sr_py_js_frame *this = (struct sr_py_js_frame*)self;
    struct sr_strbuf *buf = sr_strbuf_new();

    sr_strbuf_append_str(buf, "at ");

    if (this->frame->function_name)
    {
        sr_strbuf_append_strf(buf, "%s (", this->frame->function_name);
    }

    if (this->frame->file_name)
    {
        sr_strbuf_append_str(buf, this->frame->file_name);
    }
    else
    {
        sr_strbuf_append_str(buf, "<unknown>");
    }

    sr_strbuf_append_strf(buf, ":%d:%d", this->frame->file_line, this->frame->line_column);

    if (this->frame->function_name)
    {
        sr_strbuf_append_str(buf, ")");
    }

    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_js_frame_dup(PyObject *self, PyObject *args)
{
    struct sr_py_js_frame *this = (struct sr_py_js_frame*)self;
    struct sr_py_js_frame *fo = (struct sr_py_js_frame*)
        PyObject_New(struct sr_py_js_frame, &sr_py_js_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_js_frame_dup(this->frame, false);

    return (PyObject*)fo;
}
