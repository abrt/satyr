#include "py_base_stacktrace.h"
#include "py_java_stacktrace.h"
#include "py_java_thread.h"
#include "py_java_frame.h"
#include "java/stacktrace.h"
#include "java/thread.h"
#include "location.h"
#include "stacktrace.h"

#define stacktrace_doc "satyr.JavaStacktrace - class representing a java stacktrace\n\n" \
                      "Usage:\n\n" \
                      "satyr.JavaStacktrace() - creates an empty stacktrace\n\n" \
                      "satyr.JavaStacktrace(str) - parses str and fills the stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n\n" \
                  "Returns: satyr.JavaStacktrace - a new clone of java stacktrace\n\n" \
                  "Clones the stacktrace object. All new structures are independent " \
                  "of the original object."

static PyMethodDef
java_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  sr_py_java_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
    { NULL },
};


PyTypeObject sr_py_java_stacktrace_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.JavaStacktrace",           /* tp_name */
    sizeof(struct sr_py_java_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_java_stacktrace_free,    /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
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

PyObject *
java_stacktrace_to_python_obj(struct sr_java_stacktrace *stacktrace)
{
    struct sr_py_java_stacktrace *bo = PyObject_New(struct sr_py_java_stacktrace,
                                                    &sr_py_java_stacktrace_type);
    if (!bo)
        return PyErr_NoMemory();

    bo->thread_type = &sr_py_java_thread_type;
    bo->frame_type = &sr_py_java_frame_type;

    bo->stacktrace = stacktrace;
    bo->threads = threads_to_python_list((struct sr_stacktrace *)bo->stacktrace,
                                         bo->thread_type, bo->frame_type);
    if (!bo->threads)
        return NULL;

    return (PyObject *)bo;
}

/* constructor */
PyObject *
sr_py_java_stacktrace_new(PyTypeObject *object,
                          PyObject *args,
                          PyObject *kwds)
{
    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    struct sr_java_stacktrace *stacktrace;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        stacktrace = sr_java_stacktrace_parse(&str, &location);
        if (!stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        stacktrace = sr_java_stacktrace_new();

    return java_stacktrace_to_python_obj(stacktrace);
}

/* destructor */
void
sr_py_java_stacktrace_free(PyObject *object)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->threads);
    this->stacktrace->threads = NULL;
    sr_java_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_java_stacktrace_str(PyObject *self)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace *)self;
    GString *buf = g_string_new(NULL);
    g_string_append_printf(buf, "Java stacktrace with %zd threads",
                           (ssize_t)(PyList_Size(this->threads)));
    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    g_free(str);
    return result;
}

/* methods */
PyObject *
sr_py_java_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_java_stacktrace *this = (struct sr_py_java_stacktrace*)self;
    if (threads_prepare_linked_list((struct sr_py_multi_stacktrace *)this) < 0)
        return NULL;

    struct sr_java_stacktrace *stacktrace = sr_java_stacktrace_dup(this->stacktrace);
    if (!stacktrace)
        return NULL;

    return java_stacktrace_to_python_obj(stacktrace);
}
