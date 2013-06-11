#include "py_common.h"
#include "py_python_frame.h"
#include "py_python_stacktrace.h"
#include "utils.h"
#include "strbuf.h"
#include "python/frame.h"
#include "python/stacktrace.h"
#include "location.h"
#include "normalize.h"
#include "stacktrace.h"

#define stacktrace_doc "satyr.PythonStacktrace - class representing a python stacktrace\n" \
                      "Usage:\n" \
                      "satyr.PythonStacktrace() - creates an empty python stacktrace\n" \
                      "satyr.PythonStacktrace(str) - parses str and fills the python stacktrace object"

#define f_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: satyr.PythonStacktrace - a new clone of python stacktrace\n" \
                  "Clones the PythonStacktrace object. All new structures are independent " \
                  "on the original object."

#define f_to_short_text "Usage: stacktrace.to_short_text([max_frames])\n" \
                        "Returns short text representation of the stacktrace. If max_frames is\n" \
                        "specified, the result includes only that much topmost frames.\n"

#define f_normalize_doc "Usage: stacktrace.normalize()\n" \
                        "Normalizes the stacktrace."

#define f_frames_doc "A list containing frames"
#define f_get_modules_doc "Usage: stacktrace.get_modules()\n" \
                        "Returns: list of strings - loaded modules at time of the event"


static PyMethodDef
python_stacktrace_methods[] =
{
    /* methods */
    { "dup",           sr_py_python_stacktrace_dup,           METH_NOARGS,  f_dup_doc },
    { "to_short_text", sr_py_python_stacktrace_to_short_text, METH_VARARGS, f_to_short_text },
//    { "normalize", sr_py_python_stacktrace_normalize, METH_NOARGS, f_normalize_doc },
    { NULL },
};

static PyMemberDef
python_stacktrace_members[] =
{
    { (char *)"frames", T_OBJECT_EX, offsetof(struct sr_py_python_stacktrace, frames), 0, (char *)f_frames_doc },
    { NULL },
};

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_python_stacktrace
#define GSOFF_PY_MEMBER stacktrace
#define GSOFF_C_STRUCT sr_python_stacktrace
GSOFF_START
GSOFF_MEMBER(exception_name)
GSOFF_END

static PyGetSetDef
python_stacktrace_getset[] =
{
    SR_ATTRIBUTE_STRING(exception_name, "Exception type (string)"              ),
    { NULL },
};

PyTypeObject sr_py_python_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.PythonStacktrace",    /* tp_name */
    sizeof(struct sr_py_python_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_python_stacktrace_free,   /* tp_dealloc */
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
    sr_py_python_stacktrace_str,    /* tp_str */
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
    python_stacktrace_methods,      /* tp_methods */
    python_stacktrace_members,      /* tp_members */
    python_stacktrace_getset,       /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_python_stacktrace_new,    /* tp_new */
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
python_stacktrace_prepare_linked_list(struct sr_py_python_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    struct sr_py_python_frame *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->frames); ++i)
    {
        item = PyList_GetItem(stacktrace->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &sr_py_python_frame_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                            "frames must be a list of satyr.PythonFrame objects");
            return -1;
        }

        current = (struct sr_py_python_frame*)item;
        if (i == 0)
            stacktrace->stacktrace->frames = current->frame;
        else
            prev->frame->next = current->frame;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->frame->next = NULL;
        Py_XDECREF(current);
    }

    return 0;
}

PyObject *
python_frame_linked_list_to_python_list(struct sr_python_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_python_frame *frame = stacktrace->frames;
    struct sr_py_python_frame *item;

    while(frame)
    {
        item = (struct sr_py_python_frame*)
            PyObject_New(struct sr_py_python_frame, &sr_py_python_frame_type);

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
python_free_frame_python_list(struct sr_py_python_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    for (i = 0; i < PyList_Size(stacktrace->frames); ++i)
    {
        item = PyList_GetItem(stacktrace->frames, i);
        if (!item)
          return -1;
        Py_DECREF(item);
    }
    Py_DECREF(stacktrace->frames);

    return 0;
}

/* constructor */
PyObject *
sr_py_python_stacktrace_new(PyTypeObject *object,
                            PyObject *args,
                            PyObject *kwds)
{
    struct sr_py_python_stacktrace *bo = (struct sr_py_python_stacktrace*)
        PyObject_New(struct sr_py_python_stacktrace,
                     &sr_py_python_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        bo->stacktrace = sr_python_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }

        bo->frames = python_frame_linked_list_to_python_list(bo->stacktrace);
    }
    else
    {
        bo->stacktrace = sr_python_stacktrace_new();
        bo->frames = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
sr_py_python_stacktrace_free(PyObject *object)
{
    struct sr_py_python_stacktrace *this = (struct sr_py_python_stacktrace*)object;
    python_free_frame_python_list(this);
    this->stacktrace->frames = NULL;
    sr_python_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_python_stacktrace_str(PyObject *self)
{
    struct sr_py_python_stacktrace *this = (struct sr_py_python_stacktrace *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Python stacktrace with %zd frames",
                           (ssize_t)(PyList_Size(this->frames)));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_python_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_python_stacktrace *this = (struct sr_py_python_stacktrace*)self;
    if (python_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct sr_py_python_stacktrace *bo = (struct sr_py_python_stacktrace*)
        PyObject_New(struct sr_py_python_stacktrace,
                     &sr_py_python_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = sr_python_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->frames =  python_frame_linked_list_to_python_list(bo->stacktrace);
    if (!bo->frames)
        return NULL;

    return (PyObject*)bo;
}

PyObject *
sr_py_python_stacktrace_to_short_text(PyObject *self, PyObject *args)
{
    int max_frames = 0;
    if (!PyArg_ParseTuple(args, "|i", &max_frames))
        return NULL;

    struct sr_py_python_stacktrace *this = (struct sr_py_python_stacktrace*)self;
    if (python_stacktrace_prepare_linked_list(this) < 0)
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
 * No python normalization implemented yet
 *

PyObject *
sr_py_python_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_python_stacktrace *this = (struct sr_py_python_stacktrace*)self;

    struct sr_python_stacktrace *tmp = sr_python_stacktrace_dup(this->stacktrace);
    sr_normalize_python_stacktrace(tmp);
    if (python_free_frame_python_list(this) < 0)
    {
        sr_python_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->frames = tmp->frames;
    tmp->frames = NULL;
    sr_python_stacktrace_free(tmp);

    this->frames = python_frame_linked_list_to_python_list(this->stacktrace);
    if (!this->frames)
        return NULL;

    Py_RETURN_NONE;
}
*/
