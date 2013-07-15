#include "py_koops_frame.h"
#include "py_koops_stacktrace.h"
#include "py_base_stacktrace.h"
#include "py_common.h"
#include "strbuf.h"
#include "koops/frame.h"
#include "koops/stacktrace.h"
#include "location.h"
#include "normalize.h"
#include "stacktrace.h"
#include "internal_utils.h"

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

#define b_to_short_text "Usage: stacktrace.to_short_text([max_frames])\n" \
                        "Returns short text representation of the stacktrace. If max_frames is\n" \
                        "specified, the result includes only that much topmost frames.\n"

#define b_frames_doc "A list containing frames"
#define b_taint_flags_doc "Dictionary of kernel taint flags. Keys are the flag names,\n" \
                          "values are booleans indicating whether the flag is set."
#define b_modules_doc "Modules loaded at the time of the event (list of strings)"


static PyMethodDef
koops_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  sr_py_koops_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                   },
    { "normalize",            sr_py_koops_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc             },
    { NULL },
};

static PyGetSetDef
koops_stacktrace_getset[] =
{
    { (char*)"modules", sr_py_koops_stacktrace_get_modules, sr_py_koops_stacktrace_set_modules, (char*)b_modules_doc, NULL },
    { (char*)"taint_flags", sr_py_koops_stacktrace_get_taint_flags, sr_py_koops_stacktrace_set_taint_flags, (char*)b_taint_flags_doc, NULL },
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
    NULL,                           /* tp_members */
    koops_stacktrace_getset,        /* tp_getset */
    &sr_py_single_stacktrace_type,  /* tp_base */
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

PyObject *
sr_py_koops_stacktrace_get_modules(PyObject *self, void *data)
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

int
sr_py_koops_stacktrace_set_modules(PyObject *self, PyObject *rhs, void *data)
{
    PyErr_SetString(PyExc_NotImplementedError, "Setting module list is not implemented.");
    return -1;
}

PyObject *
sr_py_koops_stacktrace_get_taint_flags(PyObject *self, void *data)
{
    struct sr_koops_stacktrace *st = ((struct sr_py_koops_stacktrace*)self)->stacktrace;
    PyObject *flags = PyDict_New();

    struct sr_taint_flag *f;
    for (f = sr_flags; f->letter; f++)
    {
        PyObject *val = MEMB_T(bool, st, f->member_offset) ? Py_True : Py_False;
        if (PyDict_SetItemString(flags, f->name, val) == -1)
            return NULL;
    }

    return flags;
}

int
sr_py_koops_stacktrace_set_taint_flags(PyObject *self, PyObject *rhs, void *data)
{
    PyErr_SetString(PyExc_NotImplementedError, "Setting taint flags is not implemented.");
    return -1;
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

    bo->frame_type = &sr_py_koops_frame_type;

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

        bo->frames = frames_to_python_list((struct sr_thread *)bo->stacktrace, bo->frame_type);
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
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);
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
    sr_strbuf_append_strf(buf, "Kerneloops with %zd frames",
                           (ssize_t)(PyList_Size(this->frames)));
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
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    struct sr_py_koops_stacktrace *bo = (struct sr_py_koops_stacktrace*)
        PyObject_New(struct sr_py_koops_stacktrace,
                     &sr_py_koops_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->frame_type = &sr_py_koops_frame_type;
    bo->stacktrace = sr_koops_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->frames = frames_to_python_list((struct sr_thread *)bo->stacktrace, bo->frame_type);
    if (!bo->frames)
        return NULL;

    return (PyObject*)bo;
}

PyObject *
sr_py_koops_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_koops_stacktrace *this = (struct sr_py_koops_stacktrace*)self;
    if (frames_prepare_linked_list((struct sr_py_base_thread *)this) < 0)
        return NULL;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct sr_koops_stacktrace *tmp = sr_koops_stacktrace_dup(this->stacktrace);
    sr_normalize_koops_stacktrace(tmp);
    /* the list will decref all of its elements */
    Py_DECREF(this->frames);

    this->stacktrace->frames = tmp->frames;
    tmp->frames = NULL;
    sr_koops_stacktrace_free(tmp);

    this->frames = frames_to_python_list((struct sr_thread *)this->stacktrace, this->frame_type);
    if (!this->frames)
        return NULL;

    Py_RETURN_NONE;
}
