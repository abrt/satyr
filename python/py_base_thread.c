/*
    py_base_thread.c

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
#include "py_base_thread.h"
#include "py_base_frame.h"

#include "thread.h"
#include "frame.h"
#include "distance.h"

#define thread_doc "satyr.BaseThread - base class for threads"
#define frames_doc "A list containing objects representing frames in a thread."

#define distance_doc "Usage: thread.distance(other, dist_type=DISTANCE_LEVENSHTEIN)\n\n"\
                     "other: other thread\n\n"\
                     "dist_type (optional): one of DISTANCE_LEVENSHTEIN, DISTANCE_JARO_WINKLER, "\
                     "DISTANCE_JACCARD or DISTANCE_DAMERAU_LEVENSHTEIN\n\n"\
                     "Returns: positive float - distance between the two threads"

#define get_duphash_doc "Usage: thread.get_duphash(frames=0, flags=DUPHASH_NORMAL, prefix='')\n\n"\
                        "Returns: string - thread's duplication hash\n\n"\
                        "frames: integer - number of frames to use (default 0 means use all)\n\n"\
                        "flags: integer - bitwise sum of flags (DUPHASH_NORMAL, DUPHASH_NOHASH, \n"\
                        "DUPHASH_NONORMALIZE)\n\n"\
                        "prefix: string - string to be prepended in front of the text before hashing"

#define equals_doc "Usage: frame.equals(otherthread)\n\n" \
                   "Returns: bool - True if thread has attributes equal to otherthread"

static PyMethodDef
thread_methods[] =
{
    /* methods */
    { "distance",    (PyCFunction)sr_py_base_thread_distance,    METH_VARARGS|METH_KEYWORDS, distance_doc    },
    { "get_duphash", (PyCFunction)sr_py_base_thread_get_duphash, METH_VARARGS|METH_KEYWORDS, get_duphash_doc },
    { "equals",      sr_py_base_thread_equals,                   METH_VARARGS,               equals_doc      },
    { NULL },
};

static PyMemberDef
thread_members[] =
{
    { (char *)"frames", T_OBJECT_EX, offsetof(struct sr_py_base_thread, frames), 0, (char *)frames_doc },
    { NULL },
};

PyTypeObject
sr_py_base_thread_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.BaseThread",         /* tp_name */
    sizeof(struct sr_py_base_thread), /* tp_basicsize */
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
    thread_doc,                  /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    thread_methods,             /* tp_methods */
    thread_members,             /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
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

/* helpers */
int
frames_prepare_linked_list(struct sr_py_base_thread *thread)
{
    int i;
    PyObject *item;
    struct sr_py_base_frame *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, thread->frame_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_Format(PyExc_TypeError, "frames must be a list of %s objects",
                         thread->frame_type->tp_name);
            return -1;
        }

        current = (struct sr_py_base_frame*)item;
        if (i == 0)
            sr_thread_set_frames(thread->thread, current->frame);
        else
            sr_frame_set_next(prev->frame, current->frame);

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        sr_frame_set_next(current->frame, NULL);
        Py_DECREF(current);
    }

    return 0;
}

PyObject *
frames_to_python_list(struct sr_thread *thread, PyTypeObject *frame_type)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    struct sr_frame *frame = sr_thread_frames(thread);
    struct sr_py_base_frame *item;
    while (frame)
    {
        item = PyObject_New(struct sr_py_base_frame, frame_type);
        if (!item)
            return PyErr_NoMemory();

        /* XXX may need to initialize item further */
        /* It would be a good idea to have a common code that is executed each
         * time new object (e.g. via __new__ or _dup) is created so that we
         * don't miss setting some attribute. As opposed to using PyObject_New
         * directly. */
        item->frame = frame;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        frame = sr_frame_next(frame);
    }

    return result;
}

/* comparison */
static int
sr_py_base_thread_cmp(struct sr_py_base_thread *self, struct sr_py_base_thread *other)
{
    if (self->ob_type != other->ob_type)
    {
        /* distinct types must be unequal */
        return normalize_cmp(self->ob_type - other->ob_type);
    }

    if (frames_prepare_linked_list(self) < 0
        || frames_prepare_linked_list(other) < 0)
    {
        /* exception is already set */
        return -1;
    }

    return normalize_cmp(sr_thread_cmp(self->thread, other->thread));
}

PyObject *
sr_py_base_thread_equals(PyObject *self, PyObject *args)
{
    PyObject *other;

    if (!PyArg_ParseTuple(args, "O!", &sr_py_base_thread_type, &other))
        return NULL;

    if (sr_py_base_thread_cmp((struct sr_py_base_thread *)self,
                              (struct sr_py_base_thread *)other) == 0)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

/* methods */
PyObject *
sr_py_base_thread_distance(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *other;
    int dist_type = SR_DISTANCE_LEVENSHTEIN;
    static const char *kwlist[] = { "other", "dist_type", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!|i", (char **)kwlist,
                                     &sr_py_base_thread_type, &other, &dist_type))
        return NULL;

    struct sr_py_base_thread *t1 = (struct sr_py_base_thread *)self;
    struct sr_py_base_thread *t2 = (struct sr_py_base_thread *)other;

    if (frames_prepare_linked_list(t1) < 0)
        return NULL;

    if (frames_prepare_linked_list(t2) < 0)
        return NULL;

    if (self->ob_type != other->ob_type)
    {
        PyErr_SetString(PyExc_TypeError, "Both threads must have the same type");
        return NULL;
    }

    if (dist_type < 0 || dist_type >= SR_DISTANCE_NUM)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid distance type");
        return NULL;
    }

    float dist = sr_distance(dist_type, t1->thread, t2->thread);
    return PyFloat_FromDouble((double)dist);
}

PyObject *
sr_py_base_thread_get_duphash(PyObject *self, PyObject *args, PyObject *kwds)
{
    const char *prefix = NULL;
    int frames = 0, flags = 0;

    static const char *kwlist[] = { "frames", "flags", "prefix", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iis", (char **)kwlist,
                                     &frames, &flags, &prefix))
        return NULL;

    struct sr_py_base_thread *this =
        (struct sr_py_base_thread *)self;
    if (frames_prepare_linked_list(this) < 0)
        return NULL;

    char *hash = sr_thread_get_duphash((struct sr_thread *)this->thread,
                                       frames, (char *)prefix, flags);
    if (!hash)
    {
        PyErr_SetString(PyExc_RuntimeError, "cannot obtain duphash");
        return NULL;
    }

    PyObject *result = PyString_FromString(hash);
    free(hash);
    return result;
}
