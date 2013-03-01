#include "py_koops_frame.h"
#include "py_koops_stacktrace.h"
#include "lib/strbuf.h"
#include "lib/koops_frame.h"
#include "lib/koops_stacktrace.h"
#include "lib/location.h"
#include "lib/normalize.h"

#define stacktrace_doc "satyr.Kerneloops - class representing a kerneloops stacktrace\n" \
                      "Usage:\n" \
                      "satyr.Kerneloops() - creates an empty kerneloops stacktrace\n" \
                      "satyr.Kerneloops(str) - parses str and fills the kerneloops stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: satyr.Kerneloops - a new clone of kerneloops stacktrace\n" \
                  "Clones the kerneloops object. All new structures are independent " \
                  "on the original object."

#define b_normalize_doc "Usage: stacktrace.normalize()\n" \
                        "Normalizes the stacktrace."

#define b_frames_doc "A list containing frames"
#define b_get_modules_doc "Usage: stacktrace.get_modules()\n" \
                        "Returns: list of strings - loaded modules at time of the event"


static PyMethodDef
koops_stacktrace_methods[] =
{
    /* getters & setters */
    { "get_modules",          sr_py_koops_stacktrace_get_modules,          METH_NOARGS,  b_get_modules_doc           },
    /* methods */
    { "dup",                  sr_py_koops_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                   },
    { "normalize",            sr_py_koops_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc             },
    { NULL },
};

static PyMemberDef
koops_stacktrace_members[] =
{
    { (char *)"frames",       T_OBJECT_EX, offsetof(struct sr_py_koops_stacktrace, frames),     0,    b_frames_doc     },
    { NULL },
};

PyTypeObject sr_py_koops_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.Kerneloops",          /* tp_name */
    sizeof(struct sr_py_koops_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_koops_stacktrace_free,    /* tp_dealloc */
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
    sr_py_koops_stacktrace_str,     /* tp_str */
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
    koops_stacktrace_methods,       /* tp_methods */
    koops_stacktrace_members,       /* tp_members */
    NULL,                           /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_koops_stacktrace_new,     /* tp_new */
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
koops_stacktrace_prepare_linked_list(struct sr_py_koops_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    struct sr_py_koops_frame *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->frames); ++i)
    {
        item = PyList_GetItem(stacktrace->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &sr_py_koops_frame_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                            "frames must be a list of satyr.KerneloopsFrame objects");
            return -1;
        }

        current = (struct sr_py_koops_frame*)item;
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
sr_py_koops_stacktrace_get_modules(PyObject *self, PyObject *args) 
{
    struct sr_koops_stacktrace *st = ((struct sr_py_koops_stacktrace*)self)->stacktrace;
    char **iter = st->modules;

    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    while(iter && *iter)
    {
      PyList_Append(result, Py_BuildValue("s", *iter++));
    }

    return result;
}

PyObject *
koops_frame_linked_list_to_python_list(struct sr_koops_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_koops_frame *frame = stacktrace->frames;
    struct sr_py_koops_frame *item;

    while(frame)
    {
        item = (struct sr_py_koops_frame*)
            PyObject_New(struct sr_py_koops_frame, &sr_py_koops_frame_type);

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
koops_free_frame_python_list(struct sr_py_koops_stacktrace *stacktrace)
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
sr_py_koops_stacktrace_new(PyTypeObject *object,
                           PyObject *args,
                           PyObject *kwds)
{
    struct sr_py_koops_stacktrace *bo = (struct sr_py_koops_stacktrace*)
        PyObject_New(struct sr_py_koops_stacktrace,
                     &sr_py_koops_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        bo->stacktrace = sr_koops_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }

        bo->frames = koops_frame_linked_list_to_python_list(bo->stacktrace);
    }
    else
    {
        bo->stacktrace = sr_koops_stacktrace_new();
        bo->frames = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
sr_py_koops_stacktrace_free(PyObject *object)
{
    struct sr_py_koops_stacktrace *this = (struct sr_py_koops_stacktrace*)object;
    koops_free_frame_python_list(this);
    this->stacktrace->frames = NULL;
    sr_koops_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_koops_stacktrace_str(PyObject *self)
{
    struct sr_py_koops_stacktrace *this = (struct sr_py_koops_stacktrace *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Kerneloops with %d frames",
                           PyList_Size(this->frames));
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_koops_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_koops_stacktrace *this = (struct sr_py_koops_stacktrace*)self;
    if (koops_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct sr_py_koops_stacktrace *bo = (struct sr_py_koops_stacktrace*)
        PyObject_New(struct sr_py_koops_stacktrace,
                     &sr_py_koops_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = sr_koops_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->frames = koops_frame_linked_list_to_python_list(bo->stacktrace);
    if (!bo->frames)
        return NULL;

    return (PyObject*)bo;
}

PyObject *
sr_py_koops_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_koops_stacktrace *this = (struct sr_py_koops_stacktrace*)self;
    if (koops_stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct sr_koops_stacktrace *tmp = sr_koops_stacktrace_dup(this->stacktrace);
    sr_normalize_koops_stacktrace(tmp);
    if (koops_free_frame_python_list(this) < 0)
    {
        sr_koops_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->frames = tmp->frames;
    tmp->frames = NULL;
    sr_koops_stacktrace_free(tmp);

    this->frames = koops_frame_linked_list_to_python_list(this->stacktrace);
    if (!this->frames)
        return NULL;

    Py_RETURN_NONE;
}
