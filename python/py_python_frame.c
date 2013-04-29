/*
    py_python_frame.c

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
#include "py_python_frame.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/utils.h"
#include "lib/python_frame.h"

#define frame_doc "satyr.PythonFrame - class representing a python frame\n" \
                  "Usage:\n" \
                  "satyr.PythonFrame() - creates an empty frame\n" \
                  "satyr.PythonFrame(str) - parses str and fills the frame object"

#define f_dup_doc "Usage: frame.dup()\n" \
                  "Returns: satyr.PythonFrame - a new clone of frame\n" \
                  "Clones the frame object. All new structures are independent " \
                  "on the original object."


static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "dup", sr_py_python_frame_dup, METH_NOARGS,  f_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_python_frame
#define GSOFF_PY_MEMBER frame
#define GSOFF_C_STRUCT sr_python_frame
GSOFF_START
GSOFF_MEMBER(file_name),
GSOFF_MEMBER(file_line),
GSOFF_MEMBER(is_module),
GSOFF_MEMBER(function_name),
GSOFF_MEMBER(line_contents)
GSOFF_END

static PyGetSetDef
frame_getset[] =
{
    SR_ATTRIBUTE_STRING(file_name,     "Source file name (string)"                       ),
    SR_ATTRIBUTE_UINT32(file_line,     "Source line number (positive integer)"           ),
    SR_ATTRIBUTE_BOOL  (is_module,     "True if the frame is from the main module (bool)"),
    SR_ATTRIBUTE_STRING(function_name, "Function name (string)"                          ),
    SR_ATTRIBUTE_STRING(line_contents, "Remaining line contents (string)"                ),
    { NULL },
};

PyTypeObject
sr_py_python_frame_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.PythonFrame",     /* tp_name */
    sizeof(struct sr_py_python_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_python_frame_free,    /* tp_dealloc */
    NULL,                       /* tp_print */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    sr_py_python_frame_cmp,     /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    sr_py_python_frame_str,     /* tp_str */
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
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_python_frame_new,     /* tp_new */
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
sr_py_python_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_python_frame *fo = (struct sr_py_python_frame*)
        PyObject_New(struct sr_py_python_frame, &sr_py_python_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        fo->frame = sr_python_frame_parse(&str, &location);

        if (!fo->frame)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        fo->frame = sr_python_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
sr_py_python_frame_free(PyObject *object)
{
    struct sr_py_python_frame *this = (struct sr_py_python_frame*)object;
    sr_python_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_python_frame_str(PyObject *self)
{
    struct sr_py_python_frame *this = (struct sr_py_python_frame*)self;
    struct sr_strbuf *buf = sr_strbuf_new();


    if (this->frame->file_name)
      sr_strbuf_append_strf(buf, "File \"%s\"", this->frame->file_name);

    if (this->frame->file_line)
      sr_strbuf_append_strf(buf, ", %d", this->frame->file_line);

    if (this->frame->function_name)
      sr_strbuf_append_strf(buf, ", in %s", this->frame->function_name);

    if (this->frame->is_module)
      sr_strbuf_append_str(buf, ", in <module>");

    if (this->frame->line_contents)
      sr_strbuf_append_strf(buf, "\n    %s", this->frame->line_contents);

    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_python_frame_dup(PyObject *self, PyObject *args)
{
    struct sr_py_python_frame *this = (struct sr_py_python_frame*)self;
    struct sr_py_python_frame *fo = (struct sr_py_python_frame*)
        PyObject_New(struct sr_py_python_frame, &sr_py_python_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_python_frame_dup(this->frame, false);

    return (PyObject*)fo;
}

int
sr_py_python_frame_cmp(PyObject *self, PyObject *other)
{
    struct sr_python_frame *f1 = ((struct sr_py_python_frame*)self)->frame;
    struct sr_python_frame *f2 = ((struct sr_py_python_frame*)other)->frame;

    return normalize_cmp(sr_python_frame_cmp(f1, f2));
}
