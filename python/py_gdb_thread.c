#include "py_common.h"
#include "py_base_thread.h"
#include "py_gdb_thread.h"
#include "py_gdb_frame.h"
#include "strbuf.h"
#include "gdb/thread.h"
#include "gdb/frame.h"
#include "location.h"

#define thread_doc "satyr.GdbThread - class representing a thread in a stacktrace\n\n" \
                   "Usage:\n\n" \
                   "satyr.GdbThread() - creates an empty thread\n\n" \
                   "satyr.GdbThread(str) - parses str and fills the thread object\n\n" \
                   "satyr.GdbThread(str, only_funs=True) - parses list of function names"

#define t_dup_doc "Usage: thread.dup()\n\n" \
                  "Returns: satyr.GdbThread - a new clone of thread\n\n" \
                  "Clones the thread object. All new structures are independent " \
                  "of the original object."

#define t_quality_counts_doc "Usage: thread.quality_counts()\n\n" \
                             "Returns: tuple (ok, all) - ok representing number of " \
                             "'good' frames, all representing total number of frames\n\n" \
                             "Counts the number of 'good' frames and the number of all " \
                             "frames. 'Good' means the function name is known (not just '?\?')."

#define t_quality_doc "Usage: thread.quality()\n\n" \
                      "Returns: float - 0..1, thread quality\n\n" \
                      "Computes the ratio #good / #all. See quality_counts method for more."

#define t_format_funs_doc "Usage: thread.format_funs()\n\n" \
                          "Returns: string"

static PyMethodDef
gdb_thread_methods[] =
{
    /* methods */
    { "dup",            sr_py_gdb_thread_dup,            METH_NOARGS,  t_dup_doc            },
    { "quality_counts", sr_py_gdb_thread_quality_counts, METH_NOARGS,  t_quality_counts_doc },
    { "quality",        sr_py_gdb_thread_quality,        METH_NOARGS,  t_quality_doc        },
    { "format_funs",    sr_py_gdb_thread_format_funs,    METH_NOARGS, t_format_funs_doc    },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_gdb_thread
#define GSOFF_PY_MEMBER thread
#define GSOFF_C_STRUCT sr_gdb_thread
GSOFF_START
GSOFF_MEMBER(number)
GSOFF_END

static PyGetSetDef
gdb_thread_getset[] =
{
    SR_ATTRIBUTE_UINT32(number, "Thread number (positive integer)"),
    { NULL },
};

PyTypeObject sr_py_gdb_thread_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.GdbThread",          /* tp_name */
    sizeof(struct sr_py_gdb_thread),       /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_gdb_thread_free,      /* tp_dealloc */
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
    sr_py_gdb_thread_str,       /* tp_str */
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
    gdb_thread_methods,         /* tp_methods */
    NULL,                       /* tp_members */
    gdb_thread_getset,          /* tp_getset */
    &sr_py_base_thread_type,    /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_gdb_thread_new,       /* tp_new */
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
sr_py_gdb_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_gdb_thread *to = (struct sr_py_gdb_thread*)
        PyObject_New(struct sr_py_gdb_thread,
                     &sr_py_gdb_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->frame_type = &sr_py_gdb_frame_type;

    const char *str = NULL;
    int only_funs = 0;
    if (!PyArg_ParseTuple(args, "|si", &str, &only_funs))
        return NULL;

    if (str)
    {
        if (!only_funs)
        {
            struct sr_location location;
            sr_location_init(&location);
            to->thread = sr_gdb_thread_parse(&str, &location);
            if (!to->thread)
            {
                PyErr_SetString(PyExc_ValueError, location.message);
                return NULL;
            }
        }
        else
        {
            to->thread = sr_gdb_thread_parse_funs(str);
        }
        to->frames = frames_to_python_list((struct sr_thread*)to->thread,
                                           to->frame_type);
        if (!to->frames)
            return NULL;
    }
    else
    {
        to->frames = PyList_New(0);
        to->thread = sr_gdb_thread_new();
    }

    return (PyObject *)to;
}

/* destructor */
void
sr_py_gdb_thread_free(PyObject *object)
{
    struct sr_py_gdb_thread *this = (struct sr_py_gdb_thread *)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);
    this->thread->frames = NULL;
    sr_gdb_thread_free(this->thread);
    PyObject_Del(object);
}

PyObject *
sr_py_gdb_thread_str(PyObject *self)
{
    struct sr_py_gdb_thread *this = (struct sr_py_gdb_thread *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Thread #%u with %zd frames",
                           this->thread->number,
                           (ssize_t)(PyList_Size(this->frames)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_gdb_thread_dup(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_thread *this = (struct sr_py_gdb_thread *)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread*) this) < 0)
        return NULL;

    struct sr_py_gdb_thread *to = (struct sr_py_gdb_thread*)
        PyObject_New(struct sr_py_gdb_thread, &sr_py_gdb_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->frame_type = &sr_py_gdb_frame_type;

    to->thread = sr_gdb_thread_dup(this->thread, false);
    if (!to->thread)
        return NULL;

    to->frames = frames_to_python_list((struct sr_thread*)to->thread,
                                       to->frame_type);

    return (PyObject *)to;
}

PyObject *
sr_py_gdb_thread_quality_counts(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_thread *this = (struct sr_py_gdb_thread *)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    int ok = 0, all = 0;
    sr_gdb_thread_quality_counts(this->thread, &ok, &all);
    return Py_BuildValue("(ii)", ok, all);
}

PyObject *
sr_py_gdb_thread_quality(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_thread *this = (struct sr_py_gdb_thread *)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    return Py_BuildValue("f", sr_gdb_thread_quality(this->thread));
}

PyObject *
sr_py_gdb_thread_format_funs(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", sr_gdb_thread_format_funs(((struct sr_py_gdb_thread *)self)->thread));
}
