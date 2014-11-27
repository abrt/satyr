/*
    py_core_stacktrace.c

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
#include "py_base_stacktrace.h"
#include "py_core_stacktrace.h"
#include "py_core_thread.h"
#include "py_core_frame.h"
#include "py_common.h"
#include "strbuf.h"
#include "core/stacktrace.h"
#include "core/thread.h"
#include "stacktrace.h"

#define stacktrace_doc "satyr.CoreStacktrace - class representing a core stacktrace\n\n" \
                       "Usage:\n\n" \
                       "satyr.CoreStacktrace() - creates an empty stacktrace\n\n" \
                       "satyr.CoreStacktrace(json) - creates stacktrace object from JSON string"

#define b_dup_doc "Usage: stacktrace.dup()\n\n" \
                  "Returns: satyr.CoreStacktrace - a new clone of core stacktrace\n\n" \
                  "Clones the stacktrace object. All new structures are independent " \
                  "of the original object."

static PyMethodDef
core_stacktrace_methods[] =
{
    /* methods */
    { "dup", sr_py_core_stacktrace_dup, METH_NOARGS, b_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_core_stacktrace
#define GSOFF_PY_MEMBER stacktrace
#define GSOFF_C_STRUCT sr_core_stacktrace
GSOFF_START
GSOFF_MEMBER(signal),
GSOFF_MEMBER(executable),
GSOFF_MEMBER(only_crash_thread)
GSOFF_END

static PyGetSetDef
stacktrace_getset[] =
{
    SR_ATTRIBUTE_UINT16(signal,          "Signal number (int)"                     ),
    SR_ATTRIBUTE_STRING(executable,      "Name of the executable (string)"         ),
    SR_ATTRIBUTE_BOOL(only_crash_thread, "Do we only have the crash thread? (bool)"),
    { NULL }
};

PyTypeObject sr_py_core_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.CoreStacktrace",         /* tp_name */
    sizeof(struct sr_py_core_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_core_stacktrace_free,     /* tp_dealloc */
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
    sr_py_core_stacktrace_str,      /* tp_str */
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
    core_stacktrace_methods,        /* tp_methods */
    NULL,                           /* tp_members */
    stacktrace_getset,              /* tp_getset */
    &sr_py_multi_stacktrace_type,   /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_core_stacktrace_new,      /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

PyObject *
core_stacktrace_to_python_obj(struct sr_core_stacktrace *stacktrace)
{
    struct sr_py_core_stacktrace *bo = PyObject_New(struct sr_py_core_stacktrace,
                                                    &sr_py_core_stacktrace_type);
    if (!bo)
        return PyErr_NoMemory();

    bo->thread_type = &sr_py_core_thread_type;
    bo->frame_type = &sr_py_core_frame_type;

    bo->stacktrace = stacktrace;
    bo->threads = threads_to_python_list((struct sr_stacktrace *)bo->stacktrace,
                                         bo->thread_type, bo->frame_type);
    if (!bo->threads)
        return NULL;

    return (PyObject *)bo;
}

/* constructor */
PyObject *
sr_py_core_stacktrace_new(PyTypeObject *object,
                          PyObject *args,
                          PyObject *kwds)
{
    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    struct sr_core_stacktrace *stacktrace;

    if (str)
    {
        char *error_msg;
        stacktrace = sr_core_stacktrace_from_json_text(str, &error_msg);
        if (!stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, error_msg);
            free(error_msg);
            return NULL;
        }
    }
    else
        stacktrace = sr_core_stacktrace_new();

    return core_stacktrace_to_python_obj(stacktrace);
}

/* destructor */
void
sr_py_core_stacktrace_free(PyObject *object)
{
    struct sr_py_core_stacktrace *this = (struct sr_py_core_stacktrace*)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->threads);
    this->stacktrace->threads = NULL;
    sr_core_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_core_stacktrace_str(PyObject *self)
{
    struct sr_py_core_stacktrace *this = (struct sr_py_core_stacktrace *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Core stacktrace with %zd threads",
                           (ssize_t)(PyList_Size(this->threads)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_core_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_core_stacktrace *this = (struct sr_py_core_stacktrace*)self;
    if (threads_prepare_linked_list((struct sr_py_multi_stacktrace *)this) < 0)
        return NULL;

    struct sr_core_stacktrace *stacktrace = sr_core_stacktrace_dup(this->stacktrace);
    if (!stacktrace)
        return NULL;

    return core_stacktrace_to_python_obj(stacktrace);
}
