/*
    py_base_stacktrace.c

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

#include "py_base_thread.h"
#include "py_base_stacktrace.h"

#include "stacktrace.h"
#include "thread.h"

#define single_stacktrace_doc "satyr.SingleThreadStacktrace - base class for stacktrace with single thread"
#define multi_stacktrace_doc "satyr.MultiThreadStacktrace - base class for stacktrace with multiple threads"

#define to_short_text_doc "Usage: stacktrace.to_short_text([max_frames])\n" \
                          "Returns short text representation of the stacktrace. If max_frames is\n" \
                          "specified, the result includes only that much topmost frames.\n"

#define get_bthash_doc "Usage: stacktrace.get_bthash([flags])\n" \
                       "Returns: string - hash of the stacktrace\n" \
                       "flags: integer - bitwise sum of flags (BTHASH_NORMAL, BTHASH_NOHASH)"

#define threads_doc "A list containing the objects representing threads in the stacktrace."

static PyMethodDef
single_methods[] =
{
    { "to_short_text", sr_py_single_stacktrace_to_short_text, METH_VARARGS, to_short_text_doc },
    { "get_bthash",    sr_py_single_stacktrace_get_bthash,    METH_VARARGS, get_bthash_doc    },
    { NULL },
};

PyTypeObject sr_py_single_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.SingleThreadStacktrace", /* tp_name */
    sizeof(struct sr_py_base_thread), /* tp_basicsize */
    0,                              /* tp_itemsize */
    NULL,                           /* tp_dealloc */
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
    NULL,                           /* tp_str */
    NULL,                           /* tp_getattro */
    NULL,                           /* tp_setattro */
    NULL,                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    single_stacktrace_doc,          /* tp_doc */
    NULL,                           /* tp_traverse */
    NULL,                           /* tp_clear */
    NULL,                           /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    NULL,                           /* tp_iter */
    NULL,                           /* tp_iternext */
    single_methods,                 /* tp_methods */
    NULL,                           /* tp_members */
    NULL,                           /* tp_getset */
    &sr_py_base_thread_type,        /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    NULL,                           /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

static PyMemberDef
multi_members[] =
{
    { (char *)"threads", T_OBJECT_EX, offsetof(struct sr_py_multi_stacktrace, threads), 0, (char *)threads_doc },
    { NULL },
};

static PyMethodDef
multi_methods[] =
{
    { "to_short_text", sr_py_multi_stacktrace_to_short_text, METH_VARARGS, to_short_text_doc },
    { "get_bthash",    sr_py_multi_stacktrace_get_bthash,    METH_VARARGS, get_bthash_doc    },
    { NULL },
};

PyTypeObject sr_py_multi_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.MultiThreadStacktrace",  /* tp_name */
    sizeof(struct sr_py_multi_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    NULL,                           /* tp_dealloc */
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
    NULL,                           /* tp_str */
    NULL,                           /* tp_getattro */
    NULL,                           /* tp_setattro */
    NULL,                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    multi_stacktrace_doc,           /* tp_doc */
    NULL,                           /* tp_traverse */
    NULL,                           /* tp_clear */
    NULL,                           /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    NULL,                           /* tp_iter */
    NULL,                           /* tp_iternext */
    multi_methods,                  /* tp_methods */
    multi_members,                  /* tp_members */
    NULL,                           /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    NULL,                           /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

int
threads_prepare_linked_list(struct sr_py_multi_stacktrace *stacktrace)
{
    int i;
    PyObject *item;
    struct sr_py_base_thread *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(stacktrace->threads); ++i)
    {
        item = PyList_GetItem(stacktrace->threads, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, stacktrace->thread_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_Format(PyExc_TypeError,
                         "threads must be a list of %s objects",
                         stacktrace->thread_type->tp_name);
            return -1;
        }

        /* Note: this line is not present in frames_prepare_linked_list. */
        current = (struct sr_py_base_thread*)item;
        if (frames_prepare_linked_list(current) < 0)
            return -1;

        if (i == 0)
            sr_stacktrace_set_threads(stacktrace->stacktrace, current->thread);
        else
            sr_thread_set_next(prev->thread, current->thread);

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        sr_thread_set_next(current->thread, NULL);
        Py_XDECREF(current);
    }

    return 0;
}

int
threads_free_python_list(struct sr_py_multi_stacktrace *stacktrace)
{
    int i;
    PyObject *item;
    for (i = 0; i < PyList_Size(stacktrace->threads); ++i)
    {
        item = PyList_GetItem(stacktrace->threads, i);
        if (!item)
            return -1;
        Py_DECREF(item);
    }
    Py_DECREF(stacktrace->threads);

    return 0;
}

PyObject *
threads_to_python_list(struct sr_stacktrace *stacktrace, PyTypeObject *thread_type, PyTypeObject *frame_type)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_thread *thread = sr_stacktrace_threads(stacktrace);
    struct sr_py_base_thread *item;
    while (thread)
    {
        item = PyObject_New(struct sr_py_base_thread, thread_type);
        if (!item)
            return PyErr_NoMemory();

        /* XXX may need to initialize further */
        item->thread = thread;
        item->frames = frames_to_python_list(thread, frame_type);
        item->frame_type = frame_type;
        if (!item->frames)
            return NULL;

        if (PyList_Append(result, (PyObject*)item) < 0)
            return NULL;

        thread = sr_thread_next(thread);
    }

    return result;
}

PyObject *
sr_py_single_stacktrace_to_short_text(PyObject *self, PyObject *args)
{
    int max_frames = 0;
    if (!PyArg_ParseTuple(args, "|i", &max_frames))
        return NULL;

    struct sr_py_base_thread *this = (struct sr_py_base_thread *)self;
    if (frames_prepare_linked_list(this) < 0)
        return NULL;

    char *text =
        sr_stacktrace_to_short_text((struct sr_stacktrace *)this->thread, max_frames);
    if (!text)
        return NULL;

    PyObject *result = PyString_FromString(text);

    free(text);
    return result;
}

PyObject *
sr_py_multi_stacktrace_to_short_text(PyObject *self, PyObject *args)
{
    int max_frames = 0;
    if (!PyArg_ParseTuple(args, "|i", &max_frames))
        return NULL;

    struct sr_py_multi_stacktrace *this =
        (struct sr_py_multi_stacktrace *)self;
    if (threads_prepare_linked_list(this) < 0)
        return NULL;

    char *text =
        sr_stacktrace_to_short_text((struct sr_stacktrace *)this->stacktrace, max_frames);
    if (!text)
        return NULL;

    PyObject *result = PyString_FromString(text);

    free(text);
    return result;
}

/* TODO: these two functions (and the two above) are almost the same - there
 * should be a way to not repeat the code */
PyObject *
sr_py_single_stacktrace_get_bthash(PyObject *self, PyObject *args)
{
    int flags = 0;
    if (!PyArg_ParseTuple(args, "|i", &flags))
        return NULL;

    struct sr_py_base_thread *this =
        (struct sr_py_base_thread *)self;
    if (frames_prepare_linked_list(this) < 0)
        return NULL;

    char *hash = sr_stacktrace_get_bthash((struct sr_stacktrace *)this->thread, flags);
    if (!hash)
    {
        PyErr_SetString(PyExc_RuntimeError, "cannot obtain bthash");
        return NULL;
    }

    PyObject *result = PyString_FromString(hash);
    free(hash);
    return result;
}

PyObject *
sr_py_multi_stacktrace_get_bthash(PyObject *self, PyObject *args)
{
    int flags = 0;
    if (!PyArg_ParseTuple(args, "|i", &flags))
        return NULL;

    struct sr_py_multi_stacktrace *this =
        (struct sr_py_multi_stacktrace *)self;
    if (threads_prepare_linked_list(this) < 0)
        return NULL;

    char *hash = sr_stacktrace_get_bthash((struct sr_stacktrace *)this->stacktrace, flags);
    if (!hash)
    {
        PyErr_SetString(PyExc_RuntimeError, "cannot obtain bthash");
        return NULL;
    }

    PyObject *result = PyString_FromString(hash);
    free(hash);
    return result;
}
