#include "py_base_stacktrace.h"
#include "py_java_stacktrace.h"
#include "py_java_thread.h"
#include "py_java_frame.h"
#include "strbuf.h"
#include "java/stacktrace.h"
#include "java/thread.h"
#include "location.h"
#include "normalize.h"
#include "stacktrace.h"

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

static PyMethodDef
java_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  sr_py_java_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
//    { "normalize",            sr_py_java_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc            },
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
    NULL,                           /* tp_members */
    NULL,                           /* tp_getset */
    &sr_py_multi_stacktrace_type,   /* tp_base */
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

    bo->thread_type = &sr_py_java_thread_type;
    bo->frame_type = &sr_py_java_frame_type;

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
        bo->threads = threads_to_python_list((struct sr_stacktrace *)bo->stacktrace,
                                             bo->thread_type, bo->frame_type);
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
    threads_free_python_list((struct sr_py_multi_stacktrace *)this);
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
    if (threads_prepare_linked_list((struct sr_py_multi_stacktrace *)this) < 0)
        return NULL;

    struct sr_py_java_stacktrace *bo = (struct sr_py_java_stacktrace*)
        PyObject_New(struct sr_py_java_stacktrace,
                     &sr_py_java_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->thread_type = &sr_py_java_thread_type;
    bo->frame_type = &sr_py_java_frame_type;

    bo->stacktrace = sr_java_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->threads = threads_to_python_list((struct sr_stacktrace *)bo->stacktrace,
                                         bo->thread_type, bo->frame_type);
    if (!bo->threads)
        return NULL;

    return (PyObject*)bo;
}

/*
 * Normalization not yet implemente for java stacktraces
 *
PyObject *
sr_py_java_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)self;
    if (threads_prepare_linked_list((struct sr_py_multi_stacktrace *)this) < 0)
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
