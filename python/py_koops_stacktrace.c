#include "py_koops_frame.h"
#include "py_koops_stacktrace.h"
#include "lib/strbuf.h"
#include "lib/koops_frame.h"
#include "lib/koops_stacktrace.h"
#include "lib/location.h"
#include "lib/normalize.h"

#define stacktrace_doc "btparser.Kerneloops - class representing a kerneloops stacktrace\n" \
                      "Usage:\n" \
                      "btparser.Kerneloops() - creates an empty kerneloops stacktrace\n" \
                      "btparser.Kerneloops(str) - parses str and fills the kerneloops stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: btparser.Kerneloops - a new clone of kerneloops stacktrace\n" \
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
    { "get_modules",          btp_py_koops_stacktrace_get_modules,          METH_NOARGS,  b_get_modules_doc           },
    /* methods */
    { "dup",                  btp_py_koops_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                   },
    { "normalize",            btp_py_koops_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc             },
    { NULL },
};

static PyMemberDef
koops_stacktrace_members[] =
{
    { (char *)"frames",       T_OBJECT_EX, offsetof(struct btp_py_koops_stacktrace, frames),     0,    b_frames_doc     },
    { NULL },
};

PyTypeObject btp_py_koops_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Kerneloops",          /* tp_name */
    sizeof(struct btp_py_koops_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    btp_py_koops_stacktrace_free,   /* tp_dealloc */
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
    btp_py_koops_stacktrace_str,    /* tp_str */
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
    btp_py_koops_stacktrace_new,    /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

PyObject *
btp_py_koops_stacktrace_get_modules(PyObject *self, PyObject *args) 
{
    struct btp_koops_stacktrace *st = ((struct btp_py_koops_stacktrace*)self)->stacktrace;
    char **iter = st->modules;

    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    printf("%d\n", sizeof(st->modules));
    while(iter && *iter)
    {
      PyList_Append(result, Py_BuildValue("s", *iter++));
    }

    return result;
}

PyObject *
new_frame_list(struct btp_koops_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_koops_frame *frame = stacktrace->frames;
    struct btp_py_koops_frame *item;

    while(frame)
    {
        item = (struct btp_py_koops_frame*)
            PyObject_New(struct btp_py_koops_frame, &btp_py_koops_frame_type);

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
free_frame_list(struct btp_py_koops_stacktrace *stacktrace)
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
btp_py_koops_stacktrace_new(PyTypeObject *object,
                            PyObject *args,
                            PyObject *kwds)
{
    struct btp_py_koops_stacktrace *bo = (struct btp_py_koops_stacktrace*)
        PyObject_New(struct btp_py_koops_stacktrace,
                     &btp_py_koops_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct btp_location location;
        btp_location_init(&location);
        bo->stacktrace = btp_koops_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }

        bo->frames = new_frame_list(bo->stacktrace);
    }
    else
    {
        bo->stacktrace = btp_koops_stacktrace_new();
        bo->frames = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
btp_py_koops_stacktrace_free(PyObject *object)
{
    struct btp_py_koops_stacktrace *this = (struct btp_py_koops_stacktrace*)object;
    free_frame_list(this);
    this->stacktrace->frames = NULL;
    btp_koops_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_koops_stacktrace_str(PyObject *self)
{
    struct btp_py_koops_stacktrace *this = (struct btp_py_koops_stacktrace *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Kerneloops with %d frames",
                           PyList_Size(this->frames));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
btp_py_koops_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct btp_py_koops_stacktrace *this = (struct btp_py_koops_stacktrace*)self;

    struct btp_py_koops_stacktrace *bo = (struct btp_py_koops_stacktrace*)
        PyObject_New(struct btp_py_koops_stacktrace,
                     &btp_py_koops_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = btp_koops_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->frames =  new_frame_list(bo->stacktrace);
    if (!bo->frames)
        return NULL;

    return (PyObject*)bo;
}

PyObject *
btp_py_koops_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct btp_py_koops_stacktrace *this = (struct btp_py_koops_stacktrace*)self;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct btp_koops_stacktrace *tmp = btp_koops_stacktrace_dup(this->stacktrace);
    btp_normalize_koops_stacktrace(tmp);
    if (free_frame_list(this) < 0)
    {
        btp_koops_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->frames = tmp->frames;
    tmp->frames = NULL;
    btp_koops_stacktrace_free(tmp);

    this->frames = new_frame_list(this->stacktrace);
    if (!this->frames)
        return NULL;

    Py_RETURN_NONE;
}
