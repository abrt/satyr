#include "py_python_frame.h"
#include "py_python_stacktrace.h"
#include "lib/utils.h"
#include "lib/strbuf.h"
#include "lib/python_frame.h"
#include "lib/python_stacktrace.h"
#include "lib/location.h"
#include "lib/normalize.h"

#define stacktrace_doc "btparser.PythonStacktrace - class representing a python stacktrace\n" \
                      "Usage:\n" \
                      "btparser.PythonStacktrace() - creates an empty python stacktrace\n" \
                      "btparser.PythonStacktrace(str) - parses str and fills the python stacktrace object"

#define f_get_file_name_doc "Usage: frame.get_file_name()\n" \
                                "Returns: string - file name"

#define f_set_file_name_doc "Usage: frame.set_file_name(newname)\n" \
                                "newname: string - new file name"

#define f_get_file_line_doc "Usage: frame.get_file_line()\n" \
                                "Returns: unsigned int - file line"

#define f_set_file_line_doc "Usage: frame.set_file_line(newline)\n" \
                                "newline: unsigned int - new file line"

#define f_get_exception_name_doc "Usage: frame.get_exception_name()\n" \
                                "Returns: string - function name"

#define f_set_exception_name_doc "Usage: frame.set_exception_name(newname)\n" \
                                "newname: string - new function name"

#define f_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: btparser.PythonStacktrace - a new clone of python stacktrace\n" \
                  "Clones the PythonStacktrace object. All new structures are independent " \
                  "on the original object."

#define f_normalize_doc "Usage: stacktrace.normalize()\n" \
                        "Normalizes the stacktrace."

#define f_frames_doc "A list containing frames"
#define f_get_modules_doc "Usage: stacktrace.get_modules()\n" \
                        "Returns: list of strings - loaded modules at time of the event"


static PyMethodDef
python_stacktrace_methods[] =
{
    /* getters & setters */
    { "get_file_name",             btp_py_python_stacktrace_get_file_name,         METH_NOARGS,      f_get_file_name_doc             },
    { "set_file_name",             btp_py_python_stacktrace_set_file_name,         METH_VARARGS,     f_set_file_name_doc             },
    { "get_file_line",             btp_py_python_stacktrace_get_file_line,         METH_NOARGS,      f_get_file_line_doc             },
    { "set_file_line",             btp_py_python_stacktrace_set_file_line,         METH_VARARGS,     f_set_file_line_doc             },
    { "get_exception_name",        btp_py_python_stacktrace_get_exception_name,    METH_NOARGS,      f_get_exception_name_doc        },
    { "set_exception_name",        btp_py_python_stacktrace_set_exception_name,    METH_VARARGS,     f_set_exception_name_doc        },
    /* methods */
    { "dup",                       btp_py_python_stacktrace_dup,                  METH_NOARGS,       f_dup_doc                       },
//    { "normalize",                 btp_py_python_stacktrace_normalize,            METH_NOARGS,       f_normalize_doc                 },
    { NULL },
};

static PyMemberDef
python_stacktrace_members[] =
{
    { (char *)"frames",       T_OBJECT_EX, offsetof(struct btp_py_python_stacktrace, frames),     0,  f_frames_doc     },
    { NULL },
};

PyTypeObject btp_py_python_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.PythonStacktrace",    /* tp_name */
    sizeof(struct btp_py_python_stacktrace), /* tp_basicsize */
    0,                              /* tp_itemsize */
    btp_py_python_stacktrace_free,  /* tp_dealloc */
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
    btp_py_python_stacktrace_str,   /* tp_str */
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
    NULL,                           /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    btp_py_python_stacktrace_new,   /* tp_new */
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
python_stacktrace_prepare_linked_list(struct btp_py_python_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    struct btp_py_python_frame *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->frames); ++i)
    {
        item = PyList_GetItem(stacktrace->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &btp_py_python_frame_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                            "frames must be a list of btparser.PythonFrame objects");
            return -1;
        }

        current = (struct btp_py_python_frame*)item;
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
python_frame_linked_list_to_python_list(struct btp_python_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_python_frame *frame = stacktrace->frames;
    struct btp_py_python_frame *item;

    while(frame)
    {
        item = (struct btp_py_python_frame*)
            PyObject_New(struct btp_py_python_frame, &btp_py_python_frame_type);

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
python_free_frame_python_list(struct btp_py_python_stacktrace *stacktrace)
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
btp_py_python_stacktrace_new(PyTypeObject *object,
                            PyObject *args,
                            PyObject *kwds)
{
    struct btp_py_python_stacktrace *bo = (struct btp_py_python_stacktrace*)
        PyObject_New(struct btp_py_python_stacktrace,
                     &btp_py_python_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct btp_location location;
        btp_location_init(&location);
        bo->stacktrace = btp_python_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }

        bo->frames = python_frame_linked_list_to_python_list(bo->stacktrace);
    }
    else
    {
        bo->stacktrace = btp_python_stacktrace_new();
        bo->frames = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
btp_py_python_stacktrace_free(PyObject *object)
{
    struct btp_py_python_stacktrace *this = (struct btp_py_python_stacktrace*)object;
    python_free_frame_python_list(this);
    this->stacktrace->frames = NULL;
    btp_python_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_python_stacktrace_str(PyObject *self)
{
    struct btp_py_python_stacktrace *this = (struct btp_py_python_stacktrace *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Python stacktrace with %d frames",
                           PyList_Size(this->frames));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

/* file_name */
PyObject *
btp_py_python_stacktrace_get_file_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_python_stacktrace*)self)->stacktrace->file_name);
}

PyObject *
btp_py_python_stacktrace_set_file_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_python_stacktrace *stacktrace = ((struct btp_py_python_stacktrace*)self)->stacktrace;
    free(stacktrace->file_name);
    stacktrace->file_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* file_line */
PyObject *
btp_py_python_stacktrace_get_file_line(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct btp_py_python_stacktrace*)self)->stacktrace->file_line);
}

PyObject *
btp_py_python_stacktrace_set_file_line(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "File line must not be negative.");
        return NULL;
    }

    struct btp_python_stacktrace *stacktrace = ((struct btp_py_python_stacktrace*)self)->stacktrace;
    stacktrace->file_line = newvalue;
    Py_RETURN_NONE;
}

/* exception_name */
PyObject *
btp_py_python_stacktrace_get_exception_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_python_stacktrace*)self)->stacktrace->exception_name);
}

PyObject *
btp_py_python_stacktrace_set_exception_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_python_stacktrace *stacktrace = ((struct btp_py_python_stacktrace*)self)->stacktrace;
    free(stacktrace->file_name);
    stacktrace->file_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* methods */
PyObject *
btp_py_python_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct btp_py_python_stacktrace *this = (struct btp_py_python_stacktrace*)self;
    if (python_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct btp_py_python_stacktrace *bo = (struct btp_py_python_stacktrace*)
        PyObject_New(struct btp_py_python_stacktrace,
                     &btp_py_python_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = btp_python_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->frames =  python_frame_linked_list_to_python_list(bo->stacktrace);
    if (!bo->frames)
        return NULL;

    return (PyObject*)bo;
}

/*
 * No python normalization implemented yet
 *

PyObject *
btp_py_python_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct btp_py_python_stacktrace *this = (struct btp_py_python_stacktrace*)self;

    struct btp_python_stacktrace *tmp = btp_python_stacktrace_dup(this->stacktrace);
    btp_normalize_python_stacktrace(tmp);
    if (python_free_frame_python_list(this) < 0)
    {
        btp_python_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->frames = tmp->frames;
    tmp->frames = NULL;
    btp_python_stacktrace_free(tmp);

    this->frames = python_frame_linked_list_to_python_list(this->stacktrace);
    if (!this->frames)
        return NULL;

    Py_RETURN_NONE;
}
*/
