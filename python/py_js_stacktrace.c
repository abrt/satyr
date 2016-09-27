/*
    py_js_stacktrace.c

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
#include "py_js_frame.h"
#include "py_js_stacktrace.h"
#include "py_base_stacktrace.h"
#include "utils.h"
#include "strbuf.h"
#include "js/frame.h"
#include "js/stacktrace.h"
#include "location.h"
#include "stacktrace.h"

#define stacktrace_doc "satyr.JavaScriptStacktrace - class representing a JavaScript stacktrace\n\n" \
                       "Usage:\n\n" \
                       "satyr.JavaScriptStacktrace() - creates an empty JavaScript stacktrace\n\n" \
                       "satyr.JavaScriptStacktrace(str) - parses str and fills the JavaScript stacktrace object"

#define f_dup_doc "Usage: stacktrace.dup()\n\n" \
                  "Returns: satyr.JavaScriptStacktrace - a new clone of JavaScript stacktrace\n\n" \
                  "Clones the JavaScriptStacktrace object. All new structures are independent " \
                  "of the original object."


static PyMethodDef
js_stacktrace_methods[] =
{
    /* methods */
    { "dup", sr_py_js_stacktrace_dup, METH_NOARGS, f_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_js_stacktrace
#define GSOFF_PY_MEMBER stacktrace
#define GSOFF_C_STRUCT sr_js_stacktrace
GSOFF_START
GSOFF_MEMBER(exception_name)
GSOFF_END

static PyGetSetDef
js_stacktrace_getset[] =
{
    SR_ATTRIBUTE_STRING(exception_name, "Exception type (string)"              ),
    { NULL },
};

PyTypeObject sr_py_js_stacktrace_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.JavaScriptStacktrace",   /* tp_name */
    sizeof(struct sr_py_js_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_js_stacktrace_free,       /* tp_dealloc */
    NULL,                           /* tp_print */
    NULL,                           /* tp_getattr */
    NULL,                           /* tp_setattr */
    NULL,                           /* tp_compare */
    NULL,                           /* tp_repr */
    NULL,                           /* tp_as_number */
    NULL,                           /* tp_as_sequence */
    NULL,                           /* tp_as_mapping */
    NULL,                           /* tp_hash */
    NULL,                           /* tp_call */
    sr_py_js_stacktrace_str,        /* tp_str */
    NULL,                           /* tp_getattro */
    NULL,                           /* tp_setattro */
    NULL,                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    stacktrace_doc,                 /* tp_doc */
    NULL,                           /* tp_traverse */
    NULL,                           /* tp_clear */
    NULL,                           /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    NULL,                           /* tp_iter */
    NULL,                           /* tp_iternext */
    js_stacktrace_methods,        /* tp_methods */
    NULL,                           /* tp_members */
    js_stacktrace_getset,         /* tp_getset */
    &sr_py_single_stacktrace_type,  /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_js_stacktrace_new,      /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

PyObject *
js_stacktrace_to_python_obj(struct sr_js_stacktrace *stacktrace)
{
    struct sr_py_js_stacktrace *bo = PyObject_New(struct sr_py_js_stacktrace,
                                                  &sr_py_js_stacktrace_type);
    if (!bo)
        return PyErr_NoMemory();

    bo->frame_type = &sr_py_js_frame_type;

    bo->stacktrace = stacktrace;
    bo->frames = frames_to_python_list((struct sr_thread *)bo->stacktrace,
                                       bo->frame_type);
    if (!bo->frames)
        return NULL;

    return (PyObject *)bo;
}

/* constructor */
PyObject *
sr_py_js_stacktrace_new(PyTypeObject *object,
                            PyObject *args,
                            PyObject *kwds)
{
    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    struct sr_js_stacktrace *stacktrace;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        stacktrace = sr_js_stacktrace_parse(&str, &location);
        if (!stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        stacktrace = sr_js_stacktrace_new();

    return js_stacktrace_to_python_obj(stacktrace);
}

/* destructor */
void
sr_py_js_stacktrace_free(PyObject *object)
{
    struct sr_py_js_stacktrace *this = (struct sr_py_js_stacktrace*)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);
    this->stacktrace->frames = NULL;
    sr_js_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_js_stacktrace_str(PyObject *self)
{
    struct sr_py_js_stacktrace *this = (struct sr_py_js_stacktrace *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "JavaScript stacktrace with %zd frames",
                         (ssize_t)(PyList_Size(this->frames)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_js_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_js_stacktrace *this = (struct sr_py_js_stacktrace*)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    struct sr_js_stacktrace *stacktrace = sr_js_stacktrace_dup(this->stacktrace);
    if (!stacktrace)
        return NULL;

    return js_stacktrace_to_python_obj(stacktrace);
}
