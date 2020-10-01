/*
    py_core_thread.c

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
#include "py_base_thread.h"
#include "py_core_thread.h"
#include "py_core_frame.h"
#include "core/thread.h"
#include "core/frame.h"
#include "utils.h"

#define thread_doc "satyr.CoreThread - class representing a thread in a stacktrace\n\n" \
                   "Usage: satyr.CoreThread() - creates an empty thread"

#define t_dup_doc "Usage: thread.dup()\n\n" \
                  "Returns: satyr.CoreThread - a new clone of thread\n\n" \
                  "Clones the thread object. All new structures are independent " \
                  "of the original object."

static PyMethodDef
core_thread_methods[] =
{
    /* methods */
    { "dup", sr_py_core_thread_dup, METH_NOARGS, t_dup_doc },
    { NULL },
};

PyTypeObject sr_py_core_thread_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.CoreThread",      /* tp_name */
    sizeof(struct sr_py_core_thread), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_core_thread_free,     /* tp_dealloc */
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
    sr_py_core_thread_str,      /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    thread_doc,                 /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    core_thread_methods,        /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    &sr_py_base_thread_type,    /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_core_thread_new,      /* tp_new */
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
sr_py_core_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_core_thread *to = (struct sr_py_core_thread*)
        PyObject_New(struct sr_py_core_thread,
                     &sr_py_core_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->frame_type = &sr_py_core_frame_type;
    to->frames = PyList_New(0);
    to->thread = sr_core_thread_new();

    return (PyObject *)to;
}

/* destructor */
void
sr_py_core_thread_free(PyObject *object)
{
    struct sr_py_core_thread *this = (struct sr_py_core_thread *)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);
    this->thread->frames = NULL;
    sr_core_thread_free(this->thread);
    PyObject_Del(object);
}

PyObject *
sr_py_core_thread_str(PyObject *self)
{
    struct sr_py_core_thread *this = (struct sr_py_core_thread *)self;
    GString *buf = g_string_new(NULL);
    g_string_append_printf(buf, "Thread with %zd frames", (ssize_t)(PyList_Size(this->frames)));

    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_core_thread_dup(PyObject *self, PyObject *args)
{
    struct sr_py_core_thread *this = (struct sr_py_core_thread *)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    struct sr_py_core_thread *to = (struct sr_py_core_thread*)
        PyObject_New(struct sr_py_core_thread, &sr_py_core_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->frame_type = &sr_py_core_frame_type;
    to->thread = sr_core_thread_dup(this->thread, false);
    if (!to->thread)
        return NULL;

    to->frames = frames_to_python_list((struct sr_thread *)to->thread, to->frame_type);

    return (PyObject *)to;
}
