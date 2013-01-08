#include "py_gdb_stacktrace.h"
#include "py_gdb_thread.h"
#include "py_gdb_frame.h"
#include "py_gdb_sharedlib.h"
#include "lib/strbuf.h"
#include "lib/gdb_stacktrace.h"
#include "lib/gdb_thread.h"
#include "lib/gdb_sharedlib.h"
#include "lib/location.h"
#include "lib/normalize.h"

#define stacktrace_doc "btparser.Stacktrace - class representing a stacktrace\n" \
                      "Usage:\n" \
                      "btparser.Stacktrace() - creates an empty stacktrace\n" \
                      "btparser.Stacktrace(str) - parses str and fills the stacktrace object"

#define b_dup_doc "Usage: stacktrace.dup()\n" \
                  "Returns: btparser.Stacktrace - a new clone of stacktrace\n" \
                  "Clones the stacktrace object. All new structures are independent " \
                  "on the original object."

#define b_find_crash_frame_doc "Usage: stacktrace.find_crash_frame()\n" \
                               "Returns: btparser.Frame - crash frame\n" \
                               "Finds crash frame in the stacktrace. Also sets the " \
                               "stacktrace.crashframe field."

#define b_find_crash_thread_doc "Usage: stacktrace.find_crash_thread()\n" \
                                "Returns: btparser.Thread - crash thread\n" \
                                "Finds crash thread in the stacktrace. Also sets the " \
                                "stacktrace.crashthread field."

#define b_limit_frame_depth_doc "Usage: stacktrace.limit_frame_depth(N)\n" \
                                "N: positive integer - frame depth\n" \
                                "Crops all threads to only contain first N frames."

#define b_quality_simple_doc "Usage: stacktrace.quality_simple()\n" \
                             "Returns: float - 0..1, stacktrace quality\n" \
                             "Computes the quality from stacktrace itself."

#define b_quality_complex_doc "Usage: stacktrace.quality_complex()\n" \
                              "Returns: float - 0..1, stacktrace quality\n" \
                              "Computes the quality from stacktrace, crash thread and " \
                              "frames around the crash."

#define b_get_duplication_hash_doc "Usage: stacktrace.get_duplication_hash()\n" \
                                   "Returns: string - duplication hash\n" \
                                   "Computes the duplication hash used to compare stacktraces."

#define b_find_address_doc "Usage: stacktrace.find_address(address)\n" \
                           "address: long - address to find" \
                           "Returns: btparser.Sharedlib object or None if not found\n" \
                           "Looks whether the given address belongs to a shared library."

#define b_set_libnames_doc "Usage: stacktrace.set_libnames()\n" \
                           "Sets library names according to sharedlibs data."

#define b_normalize_doc "Usage: stacktrace.normalize()\n" \
                        "Normalizes all threads in the stacktrace."

#define b_get_optimized_thread_doc "Usage: stacktrace.get_optimized_thread(max_frames)\n" \
                        "Returns thread optimized for comparison."

#define b_crashframe_doc (char *)"Readonly. By default the field contains None. After " \
                         "calling the find_crash_frame method, a reference to " \
                         "btparser.Frame object is stored into the field."

#define b_crashthread_doc (char *)"Readonly. By default the field contains None. After " \
                          "calling the find_crash_thread method, a reference to " \
                          "btparser.Thread object is stored into the field."

#define b_threads_doc (char *)"A list containing the btparser.Thread objects " \
                      "representing threads in the stacktrace."

#define b_libs_doc (char *)"A list containing the btparser.Sharedlib objects " \
                   "representing shared libraries loaded at the moment of crash."

static PyMethodDef
gdb_stacktrace_methods[] =
{
    /* methods */
    { "dup",                  btp_py_gdb_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
    { "find_crash_frame",     btp_py_gdb_stacktrace_find_crash_frame,     METH_NOARGS,  b_find_crash_frame_doc     },
    { "find_crash_thread",    btp_py_gdb_stacktrace_find_crash_thread,    METH_NOARGS,  b_find_crash_thread_doc    },
    { "limit_frame_depth",    btp_py_gdb_stacktrace_limit_frame_depth,    METH_VARARGS, b_limit_frame_depth_doc    },
    { "quality_simple",       btp_py_gdb_stacktrace_quality_simple,       METH_NOARGS,  b_quality_simple_doc       },
    { "quality_complex",      btp_py_gdb_stacktrace_quality_complex,      METH_NOARGS,  b_quality_complex_doc      },
    { "get_duplication_hash", btp_py_gdb_stacktrace_get_duplication_hash, METH_NOARGS,  b_get_duplication_hash_doc },
    { "find_address",         btp_py_gdb_stacktrace_find_address,         METH_VARARGS, b_find_address_doc         },
    { "set_libnames",         btp_py_gdb_stacktrace_set_libnames,         METH_NOARGS,  b_set_libnames_doc         },
    { "normalize",            btp_py_gdb_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc            },
    { "get_optimized_thread", btp_py_gdb_stacktrace_get_optimized_thread, METH_VARARGS, b_get_optimized_thread_doc },
    { NULL },
};

static PyMemberDef
gdb_stacktrace_members[] =
{
    { (char *)"threads",     T_OBJECT_EX, offsetof(struct btp_py_gdb_stacktrace, threads),     0,        b_threads_doc     },
    { (char *)"crashframe",  T_OBJECT_EX, offsetof(struct btp_py_gdb_stacktrace, crashframe),  READONLY, b_crashframe_doc  },
    { (char *)"crashthread", T_OBJECT_EX, offsetof(struct btp_py_gdb_stacktrace, crashthread), READONLY, b_crashthread_doc },
    { (char *)"libs",        T_OBJECT_EX, offsetof(struct btp_py_gdb_stacktrace, libs),        0,        b_libs_doc        },
    { NULL },
};

PyTypeObject btp_py_gdb_stacktrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.GdbStacktrace",           /* tp_name */
    sizeof(struct btp_py_gdb_stacktrace),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    btp_py_gdb_stacktrace_free,       /* tp_dealloc */
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
    btp_py_gdb_stacktrace_str,        /* tp_str */
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
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    btp_py_gdb_stacktrace_new,        /* tp_new */
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
stacktrace_prepare_linked_list(struct btp_py_gdb_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    /* thread */
    struct btp_py_gdb_thread *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->threads); ++i)
    {
        item = PyList_GetItem(stacktrace->threads, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &btp_py_gdb_thread_type))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                            "threads must be a list of btparser.Thread objects");
            return -1;
        }

        current = (struct btp_py_gdb_thread*)item;
        if (thread_prepare_linked_list(current) < 0)
            return -1;

        if (i == 0)
            stacktrace->stacktrace->threads = current->thread;
        else
            prev->thread->next = current->thread;

        Py_XDECREF(prev);
        prev = current;
    }

    current->thread->next = NULL;
    Py_XDECREF(current);

    /* sharedlib */
    struct btp_py_gdb_sharedlib *currentlib = NULL, *prevlib = NULL;
    for (i = 0; i < PyList_Size(stacktrace->libs); ++i)
    {
        item = PyList_GetItem(stacktrace->libs, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &btp_py_gdb_sharedlib_type))
        {
            Py_XDECREF(currentlib);
            Py_XDECREF(prevlib);
            PyErr_SetString(PyExc_TypeError,
                            "libs must be a list of btparser.Sharedlib objects");
            return -1;
        }

        currentlib = (struct btp_py_gdb_sharedlib*)item;
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

int
stacktrace_free_thread_python_list(struct btp_py_gdb_stacktrace *stacktrace)
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

int
stacktrace_free_sharedlib_python_list(struct btp_py_gdb_stacktrace *stacktrace)
{
    int i;
    PyObject *item;

    for (i = 0; i < PyList_Size(stacktrace->libs); ++i)
    {
        item = PyList_GetItem(stacktrace->libs, i);
        if (!item)
            return -1;
        Py_DECREF(item);
    }
    Py_DECREF(stacktrace->libs);

    return 0;
}

PyObject *
thread_linked_list_to_python_list(struct btp_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_gdb_thread *thread = stacktrace->threads;
    struct btp_py_gdb_thread *item;
    while (thread)
    {
        item = (struct btp_py_gdb_thread*)
            PyObject_New(struct btp_py_gdb_thread, &btp_py_gdb_thread_type);

        item->thread = thread;
        item->frames = frame_linked_list_to_python_list(thread);
        if (!item->frames)
            return NULL;

        if (PyList_Append(result, (PyObject*)item) < 0)
            return NULL;

        thread = thread->next;
    }

    return result;
}

PyObject *
sharedlib_linked_list_to_python_list(struct btp_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_gdb_sharedlib *sharedlib = stacktrace->libs;
    struct btp_py_gdb_sharedlib *item;
    while (sharedlib)
    {
        item = (struct btp_py_gdb_sharedlib*)
            PyObject_New(struct btp_py_gdb_sharedlib, &btp_py_gdb_sharedlib_type);

        item->sharedlib = sharedlib;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        sharedlib = sharedlib->next;
    }

    return result;
}

int
stacktrace_rebuild_thread_python_list(struct btp_py_gdb_stacktrace *stacktrace)
{
    struct btp_gdb_thread *newlinkedlist = btp_gdb_thread_dup(stacktrace->stacktrace->threads, true);
    if (!newlinkedlist)
        return -1;
    if (stacktrace_free_thread_python_list(stacktrace) < 0)
    {
        struct btp_gdb_thread *next;
        while (newlinkedlist)
        {
            next = newlinkedlist->next;
            btp_gdb_thread_free(newlinkedlist);
            newlinkedlist = next;
        }
        return -1;
    }
    stacktrace->stacktrace->threads = newlinkedlist;
    stacktrace->threads = thread_linked_list_to_python_list(stacktrace->stacktrace);
    return 0;
}

int
stacktrace_rebuild_sharedlib_python_list(struct btp_py_gdb_stacktrace *stacktrace)
{
    struct btp_gdb_sharedlib *newlinkedlist = btp_gdb_sharedlib_dup(stacktrace->stacktrace->libs, true);
    if (!newlinkedlist)
        return -1;
    if (stacktrace_free_sharedlib_python_list(stacktrace) < 0)
    {
        struct btp_gdb_sharedlib *next;
        while (newlinkedlist)
        {
            next = newlinkedlist->next;
            btp_gdb_sharedlib_free(newlinkedlist);
            newlinkedlist = next;
        }
        return -1;
    }
    stacktrace->stacktrace->libs = newlinkedlist;
    stacktrace->libs = sharedlib_linked_list_to_python_list(stacktrace->stacktrace);
    return 0;
}

/* constructor */
PyObject *
btp_py_gdb_stacktrace_new(PyTypeObject *object,
                          PyObject *args,
                          PyObject *kwds)
{
    struct btp_py_gdb_stacktrace *bo = (struct btp_py_gdb_stacktrace*)
        PyObject_New(struct btp_py_gdb_stacktrace,
                     &btp_py_gdb_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    bo->crashframe = (struct btp_py_gdb_frame*)Py_None;
    bo->crashthread = (struct btp_py_gdb_thread*)Py_None;
    if (str)
    {
        /* ToDo parse */
        struct btp_location location;
        btp_location_init(&location);
        bo->stacktrace = btp_gdb_stacktrace_parse(&str, &location);
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
        bo->stacktrace = btp_gdb_stacktrace_new();
        bo->libs = PyList_New(0);
    }

    return (PyObject *)bo;
}

/* destructor */
void
btp_py_gdb_stacktrace_free(PyObject *object)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)object;
    stacktrace_free_thread_python_list(this);
    stacktrace_free_sharedlib_python_list(this);
    this->stacktrace->threads = NULL;
    this->stacktrace->libs = NULL;
    btp_gdb_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_gdb_stacktrace_str(PyObject *self)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Stacktrace with %d threads",
                           PyList_Size(this->threads));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
btp_py_gdb_stacktrace_dup(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    struct btp_py_gdb_stacktrace *bo = (struct btp_py_gdb_stacktrace*)
        PyObject_New(struct btp_py_gdb_stacktrace,
                     &btp_py_gdb_stacktrace_type);

    if (!bo)
        return PyErr_NoMemory();

    bo->stacktrace = btp_gdb_stacktrace_dup(this->stacktrace);
    if (!bo->stacktrace)
        return NULL;

    bo->threads = thread_linked_list_to_python_list(bo->stacktrace);
    if (!bo->threads)
        return NULL;

    bo->libs = sharedlib_linked_list_to_python_list(bo->stacktrace);
    if (!bo->libs)
        return NULL;

    if (PyObject_TypeCheck(this->crashthread, &btp_py_gdb_thread_type))
    {
        bo->crashthread = (struct btp_py_gdb_thread *)btp_py_gdb_thread_dup((PyObject*)this->crashthread, PyTuple_New(0));
        if (!bo->crashthread)
            return NULL;
    }
    else
        bo->crashthread = (struct btp_py_gdb_thread*)Py_None;

    if (PyObject_TypeCheck(this->crashframe, &btp_py_gdb_frame_type))
    {
        bo->crashframe = (struct btp_py_gdb_frame*)btp_py_gdb_thread_dup((PyObject*)this->crashframe, PyTuple_New(0));
        if (!bo->crashframe)
            return NULL;
    }
    else
        bo->crashframe = (struct btp_py_gdb_frame*)Py_None;

    return (PyObject*)bo;
}

PyObject *
btp_py_gdb_stacktrace_find_crash_frame(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys linked list - need to rebuild python list */
    struct btp_gdb_frame *frame = btp_gdb_stacktrace_get_crash_frame(this->stacktrace);
    if (!frame)
    {
        PyErr_SetString(PyExc_LookupError, "Crash frame not found");
        return NULL;
    }

    struct btp_py_gdb_frame *result = (struct btp_py_gdb_frame*)
        PyObject_New(struct btp_py_gdb_frame, &btp_py_gdb_frame_type);

    if (!result)
        return PyErr_NoMemory();

    result->frame = frame;
    this->crashframe = result;

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject *)result;
}

PyObject *
btp_py_gdb_stacktrace_find_crash_thread(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys linked list - need to rebuild python list */
    struct btp_gdb_thread *thread = btp_gdb_stacktrace_find_crash_thread(this->stacktrace);
    if (!thread)
    {
        PyErr_SetString(PyExc_LookupError, "Crash thread not found");
        return NULL;
    }

    struct btp_py_gdb_thread *result = (struct btp_py_gdb_thread*)
        PyObject_New(struct btp_py_gdb_thread, &btp_py_gdb_thread_type);

    if (!result)
        return PyErr_NoMemory();

    result->thread = btp_gdb_thread_dup(thread, false);
    result->frames = frame_linked_list_to_python_list(result->thread);
    this->crashthread = result;

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject *)result;
}

PyObject *
btp_py_gdb_stacktrace_limit_frame_depth(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    int depth;
    if (!PyArg_ParseTuple(args, "i", &depth))
        return NULL;

    /* destroys linked list - need to rebuild python list */
    btp_gdb_stacktrace_limit_frame_depth(this->stacktrace, depth);
    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_stacktrace_quality_simple(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = btp_gdb_stacktrace_quality_simple(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *
btp_py_gdb_stacktrace_quality_complex(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = btp_gdb_stacktrace_quality_complex(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *
btp_py_gdb_stacktrace_get_duplication_hash(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    char *duphash = btp_gdb_stacktrace_get_duplication_hash(this->stacktrace);
    PyObject *result = Py_BuildValue("s", duphash);
    free(duphash);

    return result;
}

PyObject *
btp_py_gdb_stacktrace_find_address(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    unsigned long long address;
    if (!PyArg_ParseTuple(args, "l", &address))
        return NULL;

    if (address == -1)
        Py_RETURN_NONE;

    int i;
    struct btp_py_gdb_sharedlib *item;
    for (i = 0; i < PyList_Size(this->libs); ++i)
    {
        item = (struct btp_py_gdb_sharedlib*)PyList_GetItem(this->libs, i);
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
btp_py_gdb_stacktrace_set_libnames(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    btp_gdb_stacktrace_set_libnames(this->stacktrace);
    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_stacktrace_normalize(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct btp_gdb_stacktrace *tmp = btp_gdb_stacktrace_dup(this->stacktrace);
    btp_normalize_gdb_stacktrace(tmp);
    if (stacktrace_free_thread_python_list(this) < 0)
    {
        btp_gdb_stacktrace_free(tmp);
        return NULL;
    }

    this->stacktrace->threads = tmp->threads;
    tmp->threads = NULL;
    btp_gdb_stacktrace_free(tmp);

    this->threads = thread_linked_list_to_python_list(this->stacktrace);
    if (!this->threads)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_stacktrace_get_optimized_thread(PyObject *self, PyObject *args)
{
    struct btp_py_gdb_stacktrace *this = (struct btp_py_gdb_stacktrace*)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    int max_frames;
    if (!PyArg_ParseTuple(args, "i", &max_frames))
        return NULL;

    struct btp_gdb_thread *thread =
        btp_gdb_stacktrace_get_optimized_thread(this->stacktrace, max_frames);
    if (!thread)
    {
        PyErr_SetString(PyExc_LookupError, "Crash thread not found");
        return NULL;
    }

    struct btp_py_gdb_thread *result = (struct btp_py_gdb_thread*)
        PyObject_New(struct btp_py_gdb_thread, &btp_py_gdb_thread_type);

    if (!result)
        return PyErr_NoMemory();

    result->thread = thread;
    result->frames = frame_linked_list_to_python_list(result->thread);

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject*)result;
}
