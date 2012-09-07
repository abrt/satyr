#include "py_gdb_thread.h"
#include "py_gdb_frame.h"
#include "lib/strbuf.h"
#include "lib/gdb_thread.h"
#include "lib/gdb_frame.h"
#include "lib/location.h"

#define thread_doc "btparser.Thread - class representing a thread in a stacktrace\n" \
                   "Usage:\n" \
                   "btparser.Thread() - creates an empty thread\n" \
                   "btparser.Thread(str) - parses str and fills the thread object\n" \
                   "btparser.Thread(str, only_funs=True) - parses list of function names"

#define t_get_number_doc "Usage: thread.get_number()\n" \
                         "Returns: positive integer - thread number"

#define t_set_number_doc "Usage: thread.set_number(N)\n" \
                         "N: positive integer - new thread number"

#define t_dup_doc "Usage: thread.dup()\n" \
                  "Returns: btparser.Thread - a new clone of thread\n" \
                  "Clones the thread object. All new structures are independent " \
                  "on the original object."

#define t_cmp_doc "Usage: thread.cmp(thread2)\n" \
                  "thread2: btparser.Thread - another thread to compare" \
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

#define t_frames_doc (char *)"A list containing btparser.Frame objects representing " \
                     "frames in a thread."

static PyMethodDef
gdb_thread_methods[] =
{
    /* getters & setters */
    { "get_number",     btp_py_gdb_thread_get_number,     METH_NOARGS,  t_get_number_doc     },
    { "set_number",     btp_py_gdb_thread_set_number,     METH_VARARGS, t_set_number_doc     },
    /* methods */
    { "cmp",            btp_py_gdb_thread_cmp,            METH_VARARGS, t_cmp_doc            },
    { "dup",            btp_py_gdb_thread_dup,            METH_NOARGS,  t_dup_doc            },
    { "quality_counts", btp_py_gdb_thread_quality_counts, METH_NOARGS,  t_quality_counts_doc },
    { "quality",        btp_py_gdb_thread_quality,        METH_NOARGS,  t_quality_doc        },
    { "format_funs",    btp_py_gdb_thread_format_funs,    METH_NOARGS, t_format_funs_doc    },
    { NULL },
};

static PyMemberDef
gdb_thread_members[] =
{
    { (char *)"frames", T_OBJECT_EX, offsetof(struct btp_py_gdb_thread, frames), 0, t_frames_doc },
    { NULL },
};

PyTypeObject btp_py_gdb_thread_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Thread",          /* tp_name */
    sizeof(struct btp_py_gdb_thread),       /* tp_basicsize */
    0,                          /* tp_itemsize */
    btp_py_gdb_thread_free,      /* tp_dealloc */
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
    btp_py_gdb_thread_str,       /* tp_str */
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
    gdb_thread_methods,              /* tp_methods */
    gdb_thread_members,              /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    btp_py_gdb_thread_new,       /* tp_new */
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
thread_prepare_linked_list(struct btp_py_gdb_thread *thread)
{
    int i;
    PyObject *item;
    struct btp_py_gdb_frame *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, &btp_py_gdb_frame_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError, "frames must be a list of btparser.Frame objects");
            return -1;
        }

        current = (struct btp_py_gdb_frame*)item;
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
thread_free_frame_python_list(struct btp_py_gdb_thread *thread)
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
frame_linked_list_to_python_list(struct btp_gdb_thread *thread)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    struct btp_gdb_frame *frame = thread->frames;
    struct btp_py_gdb_frame *item;
    while (frame)
    {
        item = (struct btp_py_gdb_frame*)
            PyObject_New(struct btp_py_gdb_frame, &btp_py_gdb_frame_type);

        if (!item)
            return PyErr_NoMemory();

        item->frame = frame;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        frame = frame->next;
    }

    return result;
}

int
thread_rebuild_python_list(struct btp_py_gdb_thread *thread)
{
    struct btp_gdb_frame *newlinkedlist = btp_gdb_frame_dup(thread->thread->frames, true);
    if (thread_free_frame_python_list(thread) < 0)
        return -1;
    /* linked list */
    thread->thread->frames = newlinkedlist;
    /* python list */
    thread->frames = frame_linked_list_to_python_list(thread->thread);
    if (!thread->frames)
    {
        struct btp_gdb_frame *next;
        while (newlinkedlist)
        {
            next = newlinkedlist->next;
            btp_gdb_frame_free(newlinkedlist);
            newlinkedlist = next;
        }
        return -1;
    }

    return 0;
}

/* constructor */
PyObject *
btp_py_gdb_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct btp_py_gdb_thread *to = (struct btp_py_gdb_thread*)
        PyObject_New(struct btp_py_gdb_thread,
                     &btp_py_gdb_thread_type);

    if (!to)
        return PyErr_NoMemory();

    const char *str = NULL;
    int only_funs = 0;
    if (!PyArg_ParseTuple(args, "|si", &str, &only_funs))
        return NULL;

    if (str)
    {
        if (!only_funs)
        {
            struct btp_location location;
            btp_location_init(&location);
            to->thread = btp_gdb_thread_parse(&str, &location);
            if (!to->thread)
            {
                PyErr_SetString(PyExc_ValueError, location.message);
                return NULL;
            }
        }
        else
        {
            to->thread = btp_gdb_thread_parse_funs(str);
        }
        to->frames = frame_linked_list_to_python_list(to->thread);
        if (!to->frames)
            return NULL;
    }
    else
    {
        to->frames = PyList_New(0);
        to->thread = btp_gdb_thread_new();
    }

    return (PyObject *)to;
}

/* destructor */
void
btp_py_gdb_thread_free(PyObject *object)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)object;
    thread_free_frame_python_list(this);
    this->thread->frames = NULL;
    btp_gdb_thread_free(this->thread);
    PyObject_Del(object);
}

PyObject *
btp_py_gdb_thread_str(PyObject *self)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Thread #%u with %d frames",
                           this->thread->number,
                           PyList_Size(this->frames));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */
PyObject *
btp_py_gdb_thread_get_number(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    return Py_BuildValue("i", this->thread->number);
}

PyObject *
btp_py_gdb_thread_set_number(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Thread number must not be negative.");
        return NULL;
    }

    this->thread->number = newvalue;
    Py_RETURN_NONE;
}

/* methods */
PyObject *
btp_py_gdb_thread_dup(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    struct btp_py_gdb_thread *to = (struct btp_py_gdb_thread*)
        PyObject_New(struct btp_py_gdb_thread, &btp_py_gdb_thread_type);

    if (!to)
        return PyErr_NoMemory();

    to->thread = btp_gdb_thread_dup(this->thread, false);
    if (!to->thread)
        return NULL;

    to->frames = frame_linked_list_to_python_list(to->thread);

    return (PyObject *)to;
}

PyObject *
btp_py_gdb_thread_cmp(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    PyObject *compare_to;
    if (!PyArg_ParseTuple(args, "O!", &btp_py_gdb_thread_type, &compare_to))
        return NULL;

    struct btp_py_gdb_thread *cmp_to = (struct btp_py_gdb_thread *)compare_to;

    if (thread_prepare_linked_list(this) < 0)
        return NULL;
    if (thread_prepare_linked_list(cmp_to) < 0)
        return NULL;

    return Py_BuildValue("i", btp_gdb_thread_cmp(this->thread, cmp_to->thread));
}

PyObject *
btp_py_gdb_thread_quality_counts(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    int ok = 0, all = 0;
    btp_gdb_thread_quality_counts(this->thread, &ok, &all);
    return Py_BuildValue("(ii)", ok, all);
}

PyObject *
btp_py_gdb_thread_quality(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_thread *this = (struct btp_py_gdb_thread *)self;
    if (thread_prepare_linked_list(this) < 0)
        return NULL;

    return Py_BuildValue("f", btp_gdb_thread_quality(this->thread));
}

PyObject *
btp_py_gdb_thread_format_funs(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", btp_gdb_thread_format_funs(((struct btp_py_gdb_thread *)self)->thread));
}
