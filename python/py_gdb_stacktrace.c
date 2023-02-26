#include "py_common.h"
#include "py_gdb_stacktrace.h"
#include "py_gdb_thread.h"
#include "py_gdb_frame.h"
#include "py_gdb_sharedlib.h"
#include "py_base_thread.h"
#include "py_base_stacktrace.h"
#include "gdb/stacktrace.h"
#include "gdb/thread.h"
#include "gdb/frame.h"
#include "gdb/sharedlib.h"
#include "location.h"
#include "normalize.h"

#define stacktrace_doc "satyr.GdbStacktrace - class representing a stacktrace\n\n" \
                      "Usage:\n\n" \
                      "satyr.GdbStacktrace() - creates an empty stacktrace\n\n" \
                      "satyr.GdbStacktrace(str) - parses str and fills the stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n\n" \
                  "Returns: satyr.GdbStacktrace - a new clone of stacktrace\n\n" \
                  "Clones the stacktrace object. All new structures are independent " \
                  "of the original object."

#define b_find_crash_frame_doc "Usage: stacktrace.find_crash_frame()\n\n" \
                               "Returns: satyr.Frame - crash frame\n\n" \
                               "Finds crash frame in the stacktrace. Also sets the " \
                               "stacktrace.crashframe field."

#define b_limit_frame_depth_doc "Usage: stacktrace.limit_frame_depth(N)\n\n" \
                                "N: positive integer - frame depth\n\n" \
                                "Crops all threads to only contain first N frames."

#define b_quality_simple_doc "Usage: stacktrace.quality_simple()\n\n" \
                             "Returns: float - 0..1, stacktrace quality\n\n" \
                             "Computes the quality from stacktrace itself."

#define b_quality_complex_doc "Usage: stacktrace.quality_complex()\n\n" \
                              "Returns: float - 0..1, stacktrace quality\n\n" \
                              "Computes the quality from stacktrace, crash thread and " \
                              "frames around the crash."

#define b_find_address_doc "Usage: stacktrace.find_address(address)\n\n" \
                           "address: long - address to find\n\n" \
                           "Returns: satyr.Sharedlib object or None if not found\n\n" \
                           "Looks whether the given address belongs to a shared library."

#define b_set_libnames_doc "Usage: stacktrace.set_libnames()\n\n" \
                           "Sets library names according to sharedlibs data."

#define b_normalize_doc "Usage: stacktrace.normalize()\n\n" \
                        "Normalizes all threads in the stacktrace."

#define b_to_short_text "Usage: stacktrace.to_short_text([max_frames])\n\n" \
                        "Returns short text representation of the crash thread. If max_frames is\n" \
                        "specified, the result includes only that much topmost frames."

#define b_crashframe_doc (char *)"Readonly. By default the field contains None. After\n" \
                         "calling the find_crash_frame method, a reference to\n" \
                         "satyr.Frame object is stored into the field."

#define b_threads_doc (char *)"A list containing the satyr.Thread objects\n" \
                      "representing threads in the stacktrace."

#define b_libs_doc (char *)"A list containing the satyr.Sharedlib objects\n" \
                   "representing shared libraries loaded at the moment of crash."

static PyMethodDef
gdb_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  sr_py_gdb_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
    { "find_crash_frame",     sr_py_gdb_stacktrace_find_crash_frame,     METH_NOARGS,  b_find_crash_frame_doc     },
    { "limit_frame_depth",    sr_py_gdb_stacktrace_limit_frame_depth,    METH_VARARGS, b_limit_frame_depth_doc    },
    { "quality_simple",       sr_py_gdb_stacktrace_quality_simple,       METH_NOARGS,  b_quality_simple_doc       },
    { "quality_complex",      sr_py_gdb_stacktrace_quality_complex,      METH_NOARGS,  b_quality_complex_doc      },
    { "find_address",         sr_py_gdb_stacktrace_find_address,         METH_VARARGS, b_find_address_doc         },
    { "set_libnames",         sr_py_gdb_stacktrace_set_libnames,         METH_NOARGS,  b_set_libnames_doc         },
    { "normalize",            sr_py_gdb_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc            },
    { "to_short_text",        sr_py_gdb_stacktrace_to_short_text,        METH_VARARGS, b_to_short_text            },
    { NULL },
};

static PyMemberDef
gdb_stacktrace_members[] =
{
    { (char *)"crashframe",  T_OBJECT_EX, offsetof(struct sr_py_gdb_stacktrace, crashframe),  READONLY, b_crashframe_doc  },
    { (char *)"libs",        T_OBJECT_EX, offsetof(struct sr_py_gdb_stacktrace, libs),        0,        b_libs_doc        },
    { NULL },
};

PyTypeObject sr_py_gdb_stacktrace_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.GdbStacktrace",           /* tp_name */
    sizeof(struct sr_py_gdb_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    sr_py_gdb_stacktrace_free,       /* tp_dealloc */
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
    sr_py_gdb_stacktrace_str,        /* tp_str */
    NULL,                           /* tp_getattro */
    NULL,                           /* tp_setattro */
    NULL,                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    stacktrace_doc,                  /* tp_doc */
    NULL,                           /* tp_traverse */
    NULL,                           /* tp_clear */
    NULL,                           /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    NULL,                           /* tp_iter */
    NULL,                           /* tp_iternext */
    gdb_stacktrace_methods,               /* tp_methods */
    gdb_stacktrace_members,               /* tp_members */
    NULL,                           /* tp_getset */
    &sr_py_multi_stacktrace_type,   /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    sr_py_gdb_stacktrace_new,        /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

/* helpers */
static int
gdb_prepare_linked_lists(struct sr_py_gdb_stacktrace *stacktrace)
{
    if (threads_prepare_linked_list((struct sr_py_multi_stacktrace *) stacktrace) < 0)
        return -1;

    /* sharedlib */
    int i;
    PyObject *item;
    struct sr_py_gdb_sharedlib *currentlib = NULL, *prevlib = NULL;
    for (i = 0; i < PyList_Size(stacktrace->libs); ++i)
    {
        item = PyList_GetItem(stacktrace->libs, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &sr_py_gdb_sharedlib_type))
        {
            Py_XDECREF(currentlib);
            Py_XDECREF(prevlib);
            PyErr_SetString(PyExc_TypeError,
                            "libs must be a list of satyr.Sharedlib objects");
            return -1;
        }

        currentlib = (struct sr_py_gdb_sharedlib*)item;
        if (i == 0)
            stacktrace->stacktrace->libs = currentlib->sharedlib;
        else
            prevlib->sharedlib->next = currentlib->sharedlib;

        Py_XDECREF(prevlib);
        prevlib = currentlib;
    }

    if (currentlib)
    {
        currentlib->sharedlib->next = NULL;
        Py_XDECREF(currentlib);
    }

    return 0;
}

PyObject *
thread_linked_list_to_python_list(struct sr_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_gdb_thread *thread = stacktrace->threads;
    struct sr_py_gdb_thread *item;
    while (thread)
    {
        item = (struct sr_py_gdb_thread*)
            PyObject_New(struct sr_py_gdb_thread, &sr_py_gdb_thread_type);

        item->frame_type = &sr_py_gdb_frame_type;
        item->thread = thread;
        item->frames = frames_to_python_list((struct sr_thread *)thread,
                                             &sr_py_gdb_frame_type);
        if (!item->frames)
            return NULL;

        if (PyList_Append(result, (PyObject*)item) < 0)
            return NULL;

        thread = thread->next;
    }

    return result;
}

PyObject *
sharedlib_linked_list_to_python_list(struct sr_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_gdb_sharedlib *sharedlib = stacktrace->libs;
    struct sr_py_gdb_sharedlib *item;
    while (sharedlib)
    {
        item = (struct sr_py_gdb_sharedlib*)
            PyObject_New(struct sr_py_gdb_sharedlib, &sr_py_gdb_sharedlib_type);

        item->sharedlib = sharedlib;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        sharedlib = sharedlib->next;
    }

    return result;
}

/* XXX Why GDB stacktrace needs this function while other types do not? Is it
 * because the methods here modify the linked lists and other types have no
 * such methods? Shouldn't we do a copy of the linked list *before* we call the
 * C function on it?
 */
int
stacktrace_rebuild_thread_python_list(struct sr_py_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_thread *newlinkedlist = sr_gdb_thread_dup(stacktrace->stacktrace->threads, true);
    if (!newlinkedlist)
        return -1;

    /* the list will decref all of its elements */
    Py_DECREF(stacktrace->threads);

    stacktrace->stacktrace->threads = newlinkedlist;
    stacktrace->threads = threads_to_python_list((struct sr_stacktrace *)stacktrace->stacktrace,
                                                 &sr_py_gdb_thread_type, &sr_py_gdb_frame_type);
    return 0;
}

int
stacktrace_rebuild_sharedlib_python_list(struct sr_py_gdb_stacktrace *stacktrace)
{
    struct sr_gdb_sharedlib *newlinkedlist = sr_gdb_sharedlib_dup(stacktrace->stacktrace->libs, true);
    if (!newlinkedlist)
        return -1;

    /* the list will decref all of its elements */
    Py_DECREF(stacktrace->libs);
    stacktrace->stacktrace->libs = newlinkedlist;
    stacktrace->libs = sharedlib_linked_list_to_python_list(stacktrace->stacktrace);
    return 0;
}

/* constructor */
PyObject *
sr_py_gdb_stacktrace_new(PyTypeObject *object,
                          PyObject *args,
                          PyObject *kwds)
{
    struct sr_py_gdb_stacktrace *bo = (struct sr_py_gdb_stacktrace*)
        PyObject_New(struct sr_py_gdb_stacktrace,
                     &sr_py_gdb_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    bo->thread_type = &sr_py_gdb_thread_type;
    bo->frame_type = &sr_py_gdb_frame_type;
    bo->crashframe = (struct sr_py_gdb_frame*)Py_None;
    if (str)
    {
        /* ToDo parse */
        struct sr_location location;
        sr_location_init(&location);
        bo->stacktrace = sr_gdb_stacktrace_parse(&str, &location);
        if (!bo->stacktrace)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
        bo->threads = thread_linked_list_to_python_list(bo->stacktrace);
        if (!bo->threads)
            return NULL;
        bo->libs = sharedlib_linked_list_to_python_list(bo->stacktrace);
        if (!bo->libs)
            return NULL;
    }
    else
    {
        bo->threads = PyList_New(0);
        bo->stacktrace = sr_gdb_stacktrace_new();
        bo->libs = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
sr_py_gdb_stacktrace_free(PyObject *object)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)object;
    /* the list will decref all of its elements */
    Py_DECREF(this->threads);
    Py_DECREF(this->libs);
    this->stacktrace->threads = NULL;
    this->stacktrace->libs = NULL;
    sr_gdb_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_gdb_stacktrace_str(PyObject *self)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace *)self;
    GString *buf = g_string_new(NULL);
    g_string_append_printf(buf, "Stacktrace with %zd threads",
                           (ssize_t)(PyList_Size(this->threads)));
    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    g_free(str);
    return result;
}

/* methods */
PyObject *
sr_py_gdb_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    struct sr_py_gdb_stacktrace *bo = (struct sr_py_gdb_stacktrace*)
        PyObject_New(struct sr_py_gdb_stacktrace,
                     &sr_py_gdb_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->thread_type = &sr_py_gdb_thread_type;
    bo->frame_type = &sr_py_gdb_frame_type;
    bo->stacktrace = sr_gdb_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->threads = thread_linked_list_to_python_list(bo->stacktrace);
    if (!bo->threads)
        return NULL;

    bo->libs = sharedlib_linked_list_to_python_list(bo->stacktrace);
    if (!bo->libs)
        return NULL;

    if (PyObject_TypeCheck(this->crashframe, &sr_py_gdb_frame_type))
    {
        bo->crashframe = (struct sr_py_gdb_frame*)sr_py_gdb_thread_dup((PyObject*)this->crashframe, PyTuple_New(0));
        if (!bo->crashframe)
            return NULL;
    }
    else
        bo->crashframe = (struct sr_py_gdb_frame*)Py_None;

    return (PyObject*)bo;
}

PyObject *
sr_py_gdb_stacktrace_find_crash_frame(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace *)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    /* destroys linked list - need to rebuild python list */
    struct sr_gdb_frame *frame = sr_gdb_stacktrace_get_crash_frame(this->stacktrace);
    if (!frame)
    {
        PyErr_SetString(PyExc_LookupError, "Crash frame not found");
        return NULL;
    }

    struct sr_py_gdb_frame *result = (struct sr_py_gdb_frame*)
        PyObject_New(struct sr_py_gdb_frame, &sr_py_gdb_frame_type);

    if (!result)
    {
        sr_gdb_frame_free(frame);
        return PyErr_NoMemory();
    }

    result->frame = frame;
    this->crashframe = result;

    if (stacktrace_rebuild_thread_python_list(this) < 0)
    {
        sr_gdb_frame_free(frame);
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *)result;
}

PyObject *
sr_py_gdb_stacktrace_limit_frame_depth(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    int depth;
    if (!PyArg_ParseTuple(args, "i", &depth))
        return NULL;

    /* destroys linked list - need to rebuild python list */
    sr_gdb_stacktrace_limit_frame_depth(this->stacktrace, depth);
    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
sr_py_gdb_stacktrace_quality_simple(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = sr_gdb_stacktrace_quality_simple(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *
sr_py_gdb_stacktrace_quality_complex(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = sr_gdb_stacktrace_quality_complex(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *
sr_py_gdb_stacktrace_find_address(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    unsigned long long address;
    if (!PyArg_ParseTuple(args, "l", &address))
        return NULL;

    if (address == (unsigned long long) -1)
        Py_RETURN_NONE;

    int i;
    struct sr_py_gdb_sharedlib *item;
    for (i = 0; i < PyList_Size(this->libs); ++i)
    {
        item = (struct sr_py_gdb_sharedlib*)PyList_GetItem(this->libs, i);
        if (!item)
            return NULL;

        if (item->sharedlib->from <= address &&
            item->sharedlib->to >= address)
        {
            Py_INCREF(item);
            return (PyObject*)item;
        }
    }

    Py_RETURN_NONE;
}

PyObject *
sr_py_gdb_stacktrace_set_libnames(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    sr_gdb_stacktrace_set_libnames(this->stacktrace);
    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
sr_py_gdb_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct sr_gdb_stacktrace *tmp = sr_gdb_stacktrace_dup(this->stacktrace);
    sr_normalize_gdb_stacktrace(tmp);
    /* the list will decref all of its elements */
    Py_DECREF(this->threads);

    this->stacktrace->threads = tmp->threads;
    this->stacktrace->crash = tmp->crash;
    this->stacktrace->crash_tid = tmp->crash_tid;
    tmp->threads = NULL;
    tmp->crash = NULL;
    sr_gdb_stacktrace_free(tmp);

    this->threads = thread_linked_list_to_python_list(this->stacktrace);
    if (!this->threads)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
sr_py_gdb_stacktrace_to_short_text(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_stacktrace *this = (struct sr_py_gdb_stacktrace*)self;
    if (gdb_prepare_linked_lists(this) < 0)
        return NULL;

    int max_frames = 0;
    if (!PyArg_ParseTuple(args, "|i", &max_frames))
        return NULL;

    char *text =
        sr_gdb_stacktrace_to_short_text(this->stacktrace, max_frames);
    if (!text)
    {
        PyErr_SetString(PyExc_LookupError, "Crash thread not found");
        return NULL;
    }

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    PyObject *result = PyString_FromString(text);

    g_free(text);
    return result;
}
