#include "py_common.h"
#include "py_java_thread.h"
#include "py_java_frame.h"
#include "lib/strbuf.h"
#include "lib/java_thread.h"
#include "lib/java_frame.h"
#include "lib/location.h"
#include "lib/utils.h"

#define thread_doc "satyr.JavaThread - class representing a thread in a stacktrace\n" \
                   "Usage:\n" \
                   "satyr.JavaThread() - creates an empty thread\n" \
                   "satyr.JavaThread(str) - parses str and fills the thread object\n" \
                   "satyr.JavaThread(str, only_funs=True) - parses list of function names"

#define t_dup_doc "Usage: thread.dup()\n" \
                  "Returns: satyr.JavaThread - a new clone of thread\n" \
                  "Clones the thread object. All new structures are independent " \
                  "on the original object."

#define t_cmp_doc "Usage: thread.cmp(thread2)\n" \
                  "thread2: satyr.JavaThread - another thread to compare" \
                  "Returns: integer - distance" \
                  "Compares thread to thread2. Returns 0 if thread = thread2, " \
                  "<0 if thread is 'less' than thread2, >0 if thread is 'more' " \
                  "than thread2."

#define t_quality_counts_doc "Usage: thread.quality_counts()\n" \
                             "Returns: tuple (ok, all) - ok representing number of " \
                             "'good' frames, all representing total number of frames\n" \
                             "Counts the number of 'good' frames and the number of all " \
                             "frames. 'Good' means the function name is known (not just '?\?')."

#define t_quality_doc "Usage: thread.quality()\n" \
                      "Returns: float - 0..1, thread quality\n" \
                      "Computes the ratio #good / #all. See quality_counts method for more."

#define t_format_funs_doc "Usage: thread.format_funs()\n" \
                      "Returns: string"

#define t_frames_doc (char *)"A list containing satyr.JavaFrame objects representing " \
                     "frames in a thread."

static PyMethodDef
java_thread_methods[] =
{
    /* methods */
    { "cmp",            sr_py_java_thread_cmp,            METH_VARARGS, t_cmp_doc            },
    { "dup",            sr_py_java_thread_dup,            METH_NOARGS,  t_dup_doc            },
    { "quality_counts", sr_py_java_thread_quality_counts, METH_NOARGS,  t_quality_counts_doc },
    { "quality",        sr_py_java_thread_quality,        METH_NOARGS,  t_quality_doc        },
    { "format_funs",    sr_py_java_thread_format_funs,    METH_NOARGS,  t_format_funs_doc    },
    { NULL },
};

static PyMemberDef
java_thread_members[] =
{
    { (char *)"frames", T_OBJECT_EX, offsetof(struct sr_py_java_thread, frames), 0, t_frames_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_java_thread
#define GSOFF_PY_MEMBER thread
#define GSOFF_C_STRUCT sr_java_thread
GSOFF_START
GSOFF_MEMBER(name)
GSOFF_END

static PyGetSetDef
java_thread_getset[] =
{
    SR_ATTRIBUTE_STRING(name, "Thread name (string)"),
    { NULL },
};

PyTypeObject sr_py_java_thread_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.JavaThread",      /* tp_name */
    sizeof(struct sr_py_java_thread),   /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_java_thread_free,     /* tp_dealloc */
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
    sr_py_java_thread_str,      /* tp_str */
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
    java_thread_methods,        /* tp_methods */
    java_thread_members,        /* tp_members */
    java_thread_getset,         /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_java_thread_new,      /* tp_new */
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
java_thread_prepare_linked_list(struct sr_py_java_thread *thread)
{
    int i;
    PyObject *item;
    struct sr_py_java_frame *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, &sr_py_java_frame_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError, "frames must be a list of satyr.JavaFrame objects");
            return -1;
        }

        current = (struct sr_py_java_frame*)item;
        if (i == 0)
            thread->thread->frames = current->frame;
        else
            prev->frame->next = current->frame;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->frame->next = NULL;
        Py_DECREF(current);
    }

    return 0;
}

int
java_thread_free_frame_python_list(struct sr_py_java_thread *thread)
{
    int i;
    PyObject *item;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;
        Py_DECREF(item);
    }
    Py_DECREF(thread->frames);

    return 0;
}

PyObject *
java_frame_linked_list_to_python_list(struct sr_java_thread *thread)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    struct sr_java_frame *frame = thread->frames;
    struct sr_py_java_frame *item;
    while (frame)
    {
        item = (struct sr_py_java_frame*)
            PyObject_New(struct sr_py_java_frame, &sr_py_java_frame_type);

        if (!item)
            return PyErr_NoMemory();

        item->frame = frame;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        frame = frame->next;
    }

    return result;
}

/* constructor */
PyObject *
sr_py_java_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_java_thread *to = (struct sr_py_java_thread*)
        PyObject_New(struct sr_py_java_thread,
                     &sr_py_java_thread_type);

    if (!to)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        to->thread = sr_java_thread_parse(&str, &location);
        if (!to->thread)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
        to->frames = java_frame_linked_list_to_python_list(to->thread);
        if (!to->frames)
            return NULL;
    }
    else
    {
        to->frames = PyList_New(0);
        to->thread = sr_java_thread_new();
    }

    return (PyObject *)to;
}

/* destructor */
void
sr_py_java_thread_free(PyObject *object)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)object;
    java_thread_free_frame_python_list(this);
    this->thread->frames = NULL;
    sr_java_thread_free(this->thread);
    PyObject_Del(object);
}

PyObject *
sr_py_java_thread_str(PyObject *self)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_str(buf, "Thread");
    if (this->thread->name)
        sr_strbuf_append_strf(buf, " %s", this->thread->name);

    sr_strbuf_append_strf(buf, " with %zd frames", (ssize_t)(PyList_Size(this->frames)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_java_thread_dup(PyObject *self, PyObject *args)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)self;
    if (java_thread_prepare_linked_list(this) < 0)
        return NULL;

    struct sr_py_java_thread *to = (struct sr_py_java_thread*)
        PyObject_New(struct sr_py_java_thread, &sr_py_java_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->thread = sr_java_thread_dup(this->thread, false);
    if (!to->thread)
        return NULL;

    to->frames = java_frame_linked_list_to_python_list(to->thread);

    return (PyObject *)to;
}

PyObject *
sr_py_java_thread_cmp(PyObject *self, PyObject *args)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)self;
    PyObject *compare_to;
    if (!PyArg_ParseTuple(args, "O!", &sr_py_java_thread_type, &compare_to))
        return NULL;

    struct sr_py_java_thread *cmp_to = (struct sr_py_java_thread *)compare_to;

    if (java_thread_prepare_linked_list(this) < 0)
        return NULL;
    if (java_thread_prepare_linked_list(cmp_to) < 0)
        return NULL;

    return Py_BuildValue("i", sr_java_thread_cmp(this->thread, cmp_to->thread));
}

PyObject *
sr_py_java_thread_quality_counts(PyObject *self, PyObject *args)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)self;
    if (java_thread_prepare_linked_list(this) < 0)
        return NULL;

    int ok = 0, all = 0;
    sr_java_thread_quality_counts(this->thread, &ok, &all);
    return Py_BuildValue("(ii)", ok, all);
}

PyObject *
sr_py_java_thread_quality(PyObject *self, PyObject *args)
{
    struct sr_py_java_thread *this = (struct sr_py_java_thread *)self;
    if (java_thread_prepare_linked_list(this) < 0)
        return NULL;

    return Py_BuildValue("f", sr_java_thread_quality(this->thread));
}

PyObject *
sr_py_java_thread_format_funs(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", sr_java_thread_format_funs(((struct sr_py_java_thread *)self)->thread));
}
