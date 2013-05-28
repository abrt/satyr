#include "py_java_stacktrace.h"
#include "py_java_thread.h"
#include "py_java_frame.h"
#include "lib/strbuf.h"
#include "lib/java_stacktrace.h"
#include "lib/java_thread.h"
#include "lib/location.h"
#include "lib/normalize.h"
#include "lib/stacktrace.h"

#define stacktrace_doc "satyr.JavaStacktrace - class representing a java stacktrace\n" \
                      "Usage:\n" \
                      "satyr.JavaStacktrace() - creates an empty stacktrace\n" \
                      "satyr.JavaStacktrace(str) - parses str and fills the stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: satyr.JavaStacktrace - a new clone of java stacktrace\n" \
                  "Clones the stacktrace object. All new structures are independent " \
                  "on the original object."

#define b_to_short_text "Usage: stacktrace.to_short_text([max_frames])\n" \
                        "Returns short text representation of the stacktrace. If max_frames is\n" \
                        "specified, the result includes only that much topmost frames.\n"

#define b_normalize_doc "Usage: stacktrace.normalize()\n" \
                        "Normalizes all threads in the stacktrace."

#define b_threads_doc (char *)"A list containing the satyr.JavaThread objects " \
                      "representing threads in the stacktrace."

static PyMethodDef
java_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  sr_py_java_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
    { "to_short_text",        sr_py_java_stacktrace_to_short_text,        METH_VARARGS, b_to_short_text            },
//    { "normalize",            sr_py_java_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc            },
    { NULL },
};

static PyMemberDef
java_stacktrace_members[] =
{
    { (char *)"threads",     T_OBJECT_EX, offsetof(struct sr_py_java_stacktrace, threads),     0,        b_threads_doc     },
    { NULL },
};

PyTypeObject sr_py_java_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.JavaStacktrace",           /* tp_name */
    sizeof(struct sr_py_java_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_java_stacktrace_free,    /* tp_dealloc */
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
    sr_py_java_stacktrace_str,     /* tp_str */
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
    java_stacktrace_methods,        /* tp_methods */
    java_stacktrace_members,        /* tp_members */
    NULL,                           /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_java_stacktrace_new,     /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

/* helpers */
int
java_stacktrace_prepare_linked_list(struct sr_py_java_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    /* thread */
    struct sr_py_java_thread *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->threads); ++i)
    {
        item = PyList_GetItem(stacktrace->threads, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &sr_py_java_thread_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                            "threads must be a list of satyr.JavaThread objects");
            return -1;
        }

        current = (struct sr_py_java_thread*)item;
        if (java_thread_prepare_linked_list(current) < 0)
            return -1;

        if (i == 0)
            stacktrace->stacktrace->threads = current->thread;
        else
            prev->thread->next = current->thread;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->thread->next = NULL;
        Py_XDECREF(current);
    }

    return 0;
}

int
java_stacktrace_free_thread_python_list(struct sr_py_java_stacktrace *stacktrace)
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
java_thread_linked_list_to_python_list(struct sr_java_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_java_thread *thread = stacktrace->threads;
    struct sr_py_java_thread *item;
    while (thread)
    {
        item = (struct sr_py_java_thread*)
            PyObject_New(struct sr_py_java_thread, &sr_py_java_thread_type);

        item->thread = thread;
        item->frames = java_frame_linked_list_to_python_list(thread);
        if (!item->frames)
            return NULL;

        if (PyList_Append(result, (PyObject*)item) < 0)
            return NULL;

        thread = thread->next;
    }

    return result;
}

/* constructor */
PyObject *
sr_py_java_stacktrace_new(PyTypeObject *object,
                          PyObject *args,
                          PyObject *kwds)
{
    struct sr_py_java_stacktrace *bo = (struct sr_py_java_stacktrace*)
        PyObject_New(struct sr_py_java_stacktrace,
                     &sr_py_java_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        /* ToDo parse */
        struct sr_location location;
        sr_location_init(&location);
        bo->stacktrace = sr_java_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
        bo->threads = java_thread_linked_list_to_python_list(bo->stacktrace);
        if (!bo->threads)
            return NULL;
    }
    else
    {
        bo->threads = PyList_New(0);
        bo->stacktrace = sr_java_stacktrace_new();
    }

    return (PyObject *)bo;
}

/* destructor */
void
sr_py_java_stacktrace_free(PyObject *object)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)object;
    java_stacktrace_free_thread_python_list(this);
    this->stacktrace->threads = NULL;
    sr_java_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_java_stacktrace_str(PyObject *self)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Java stacktrace with %zd threads",
                           (ssize_t)(PyList_Size(this->threads)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_java_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)self;
    if (java_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct sr_py_java_stacktrace *bo = (struct sr_py_java_stacktrace*)
        PyObject_New(struct sr_py_java_stacktrace,
                     &sr_py_java_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = sr_java_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->threads = java_thread_linked_list_to_python_list(bo->stacktrace);
    if (!bo->threads)
        return NULL;

    return (PyObject*)bo;
}

PyObject *
sr_py_java_stacktrace_to_short_text(PyObject *self, PyObject *args)
{
    int max_frames = 0;
    if (!PyArg_ParseTuple(args, "|i", &max_frames))
        return NULL;

    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)self;
    if (java_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    char *text =
        sr_stacktrace_to_short_text((struct sr_stacktrace *)this->stacktrace, max_frames);
    if (!text)
        return NULL;

    PyObject *result = PyString_FromString(text);

    free(text);
    return result;
}

/*
 * Normalization not yet implemente for java stacktraces
 *
PyObject *
sr_py_java_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)self;
    if (java_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct sr_java_stacktrace *tmp = sr_java_stacktrace_dup(this->stacktrace);
    sr_normalize_java_stacktrace(tmp);
    if (java_stacktrace_free_thread_python_list(this) < 0)
    {
        sr_java_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->threads = tmp->threads;
    tmp->threads = NULL;
    sr_java_stacktrace_free(tmp);

    this->threads = java_thread_linked_list_to_python_list(this->stacktrace);
    if (!this->threads)
        return NULL;

    Py_RETURN_NONE;
}
*/
