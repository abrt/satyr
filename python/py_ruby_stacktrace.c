#include "py_common.h"
#include "py_ruby_frame.h"
#include "py_ruby_stacktrace.h"
#include "py_base_stacktrace.h"
#include "utils.h"
#include "ruby/frame.h"
#include "ruby/stacktrace.h"
#include "location.h"
#include "stacktrace.h"

#define stacktrace_doc "satyr.RubyStacktrace - class representing a ruby stacktrace\n\n" \
                       "Usage:\n\n" \
                       "satyr.RubyStacktrace() - creates an empty ruby stacktrace\n\n" \
                       "satyr.RubyStacktrace(str) - parses str and fills the ruby stacktrace object"

#define f_dup_doc "Usage: stacktrace.dup()\n\n" \
                  "Returns: satyr.RubyStacktrace - a new clone of ruby stacktrace\n\n" \
                  "Clones the RubyStacktrace object. All new structures are independent " \
                  "of the original object."


static PyMethodDef
ruby_stacktrace_methods[] =
{
    /* methods */
    { "dup", sr_py_ruby_stacktrace_dup, METH_NOARGS, f_dup_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_ruby_stacktrace
#define GSOFF_PY_MEMBER stacktrace
#define GSOFF_C_STRUCT sr_ruby_stacktrace
GSOFF_START
GSOFF_MEMBER(exception_name)
GSOFF_END

static PyGetSetDef
ruby_stacktrace_getset[] =
{
    SR_ATTRIBUTE_STRING(exception_name, "Exception type (string)"              ),
    { NULL },
};

PyTypeObject sr_py_ruby_stacktrace_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.RubyStacktrace",         /* tp_name */
    sizeof(struct sr_py_ruby_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_ruby_stacktrace_free,     /* tp_dealloc */
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
    sr_py_ruby_stacktrace_str,      /* tp_str */
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
    ruby_stacktrace_methods,        /* tp_methods */
    NULL,                           /* tp_members */
    ruby_stacktrace_getset,         /* tp_getset */
    &sr_py_single_stacktrace_type,  /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_ruby_stacktrace_new,      /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

PyObject *
ruby_stacktrace_to_python_obj(struct sr_ruby_stacktrace *stacktrace)
{
    struct sr_py_ruby_stacktrace *bo = PyObject_New(struct sr_py_ruby_stacktrace,
                                                    &sr_py_ruby_stacktrace_type);
    if (!bo)
        return PyErr_NoMemory();

    bo->frame_type = &sr_py_ruby_frame_type;

    bo->stacktrace = stacktrace;
    bo->frames = frames_to_python_list((struct sr_thread *)bo->stacktrace,
                                       bo->frame_type);
    if (!bo->frames)
        return NULL;

    return (PyObject *)bo;
}

/* constructor */
PyObject *
sr_py_ruby_stacktrace_new(PyTypeObject *object,
                            PyObject *args,
                            PyObject *kwds)
{
    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    struct sr_ruby_stacktrace *stacktrace;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        stacktrace = sr_ruby_stacktrace_parse(&str, &location);
        if (!stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        stacktrace = sr_ruby_stacktrace_new();

    return ruby_stacktrace_to_python_obj(stacktrace);
}

/* destructor */
void
sr_py_ruby_stacktrace_free(PyObject *object)
{
    struct sr_py_ruby_stacktrace *this = (struct sr_py_ruby_stacktrace*)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);
    this->stacktrace->frames = NULL;
    sr_ruby_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_ruby_stacktrace_str(PyObject *self)
{
    struct sr_py_ruby_stacktrace *this = (struct sr_py_ruby_stacktrace *)self;
    GString *buf = g_string_new(NULL);
    g_string_append_printf(buf, "Ruby stacktrace with %zd frames",
                         (ssize_t)(PyList_Size(this->frames)));
    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    g_free(str);
    return result;
}

/* methods */
PyObject *
sr_py_ruby_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_ruby_stacktrace *this = (struct sr_py_ruby_stacktrace*)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    struct sr_ruby_stacktrace *stacktrace = sr_ruby_stacktrace_dup(this->stacktrace);
    if (!stacktrace)
        return NULL;

    return ruby_stacktrace_to_python_obj(stacktrace);
}
