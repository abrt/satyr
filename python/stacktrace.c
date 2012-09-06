#include "common.h"
#include "strbuf.h"

static PyMethodDef StacktraceMethods[] = {
    /* methods */
    { "dup",                  p_btp_gdb_stacktrace_dup,                  METH_NOARGS,  b_dup_doc                  },
    { "find_crash_frame",     p_btp_gdb_stacktrace_find_crash_frame,     METH_NOARGS,  b_find_crash_frame_doc     },
    { "find_crash_thread",    p_btp_gdb_stacktrace_find_crash_thread,    METH_NOARGS,  b_find_crash_thread_doc    },
    { "limit_frame_depth",    p_btp_gdb_stacktrace_limit_frame_depth,    METH_VARARGS, b_limit_frame_depth_doc    },
    { "quality_simple",       p_btp_gdb_stacktrace_quality_simple,       METH_NOARGS,  b_quality_simple_doc       },
    { "quality_complex",      p_btp_gdb_stacktrace_quality_complex,      METH_NOARGS,  b_quality_complex_doc      },
    { "get_duplication_hash", p_btp_gdb_stacktrace_get_duplication_hash, METH_NOARGS,  b_get_duplication_hash_doc },
    { "find_address",         p_btp_gdb_stacktrace_find_address,         METH_VARARGS, b_find_address_doc         },
    { "set_libnames",         p_btp_gdb_stacktrace_set_libnames,         METH_NOARGS,  b_set_libnames_doc         },
    { "normalize",            p_btp_gdb_stacktrace_normalize,            METH_NOARGS,  b_normalize_doc            },
    { "get_optimized_thread", p_btp_gdb_stacktrace_get_optimized_thread, METH_VARARGS, b_get_optimized_thread_doc },
    { NULL },
};

static PyMemberDef StacktraceMembers[] = {
    { (char *)"threads",     T_OBJECT_EX, offsetof(StacktraceObject, threads),     0,        b_threads_doc     },
    { (char *)"crashframe",  T_OBJECT_EX, offsetof(StacktraceObject, crashframe),  READONLY, b_crashframe_doc  },
    { (char *)"crashthread", T_OBJECT_EX, offsetof(StacktraceObject, crashthread), READONLY, b_crashthread_doc },
    { (char *)"libs",        T_OBJECT_EX, offsetof(StacktraceObject, libs),        0,        b_libs_doc        },
    { NULL },
};

PyTypeObject StacktraceTypeObject = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Stacktrace",           /* tp_name */
    sizeof(StacktraceObject),        /* tp_basicsize */
    0,                              /* tp_itemsize */
    p_btp_gdb_stacktrace_free,       /* tp_dealloc */
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
    p_btp_gdb_stacktrace_str,        /* tp_str */
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
    StacktraceMethods,               /* tp_methods */
    StacktraceMembers,               /* tp_members */
    NULL,                           /* tp_getset */
    NULL,                           /* tp_base */
    NULL,                           /* tp_dict */
    NULL,                           /* tp_descr_get */
    NULL,                           /* tp_descr_set */
    0,                              /* tp_dictoffset */
    NULL,                           /* tp_init */
    NULL,                           /* tp_alloc */
    p_btp_gdb_stacktrace_new,        /* tp_new */
    NULL,                           /* tp_free */
    NULL,                           /* tp_is_gc */
    NULL,                           /* tp_bases */
    NULL,                           /* tp_mro */
    NULL,                           /* tp_cache */
    NULL,                           /* tp_subclasses */
    NULL,                           /* tp_weaklist */
};

/* helpers */
int stacktrace_prepare_linked_list(StacktraceObject *stacktrace)
{
    int i;
    PyObject *item;

    /* thread */
    ThreadObject *current = NULL, *prev = NULL;
    for (i = 0; i < PyList_Size(stacktrace->threads); ++i)
    {
        item = PyList_GetItem(stacktrace->threads, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &ThreadTypeObject))
        {
            Py_XDECREF(current);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError, "threads must be a list of btparser.Thread objects");
            return -1;
        }

        current = (ThreadObject *)item;
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
    SharedlibObject *currentlib = NULL, *prevlib = NULL;
    for (i = 0; i < PyList_Size(stacktrace->libs); ++i)
    {
        item = PyList_GetItem(stacktrace->libs, i);
        if (!item)
            return -1;

        Py_INCREF(item);
        if (!PyObject_TypeCheck(item, &SharedlibTypeObject))
        {
            Py_XDECREF(currentlib);
            Py_XDECREF(prevlib);
            PyErr_SetString(PyExc_TypeError, "libs must be a list of btparser.Sharedlib objects");
            return -1;
        }

        currentlib = (SharedlibObject *)item;
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

int stacktrace_free_thread_python_list(StacktraceObject *stacktrace)
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

int stacktrace_free_sharedlib_python_list(StacktraceObject *stacktrace)
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

PyObject *thread_linked_list_to_python_list(struct btp_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_gdb_thread *thread = stacktrace->threads;
    ThreadObject *item;
    while (thread)
    {
        item = (ThreadObject *)PyObject_New(ThreadObject, &ThreadTypeObject);
        item->thread = thread;
        item->frames = frame_linked_list_to_python_list(thread);
        if (!item->frames)
            return NULL;

        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        thread = thread->next;
    }

    return result;
}

PyObject *sharedlib_linked_list_to_python_list(struct btp_gdb_stacktrace *stacktrace)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct btp_gdb_sharedlib *sharedlib = stacktrace->libs;
    SharedlibObject *item;
    while (sharedlib)
    {
        item = (SharedlibObject *)PyObject_New(SharedlibObject, &SharedlibTypeObject);
        item->sharedlib = sharedlib;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        sharedlib = sharedlib->next;
    }

    return result;
}

int stacktrace_rebuild_thread_python_list(StacktraceObject *stacktrace)
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

int stacktrace_rebuild_sharedlib_python_list(StacktraceObject *stacktrace)
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
PyObject *p_btp_gdb_stacktrace_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    StacktraceObject *bo = (StacktraceObject *)PyObject_New(StacktraceObject, &StacktraceTypeObject);
    if (!bo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    bo->crashframe = (FrameObject *)Py_None;
    bo->crashthread = (ThreadObject *)Py_None;
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
void p_btp_gdb_stacktrace_free(PyObject *object)
{
    StacktraceObject *this = (StacktraceObject *)object;
    stacktrace_free_thread_python_list(this);
    stacktrace_free_sharedlib_python_list(this);
    this->stacktrace->threads = NULL;
    this->stacktrace->libs = NULL;
    btp_gdb_stacktrace_free(this->stacktrace);
    PyObject_Del(object);
}

/* str */
PyObject *p_btp_gdb_stacktrace_str(PyObject *self)
{
    StacktraceObject *this = (StacktraceObject *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Stacktrace with %d threads",
                           PyList_Size(this->threads));
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *p_btp_gdb_stacktrace_dup(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    StacktraceObject *bo = (StacktraceObject *)PyObject_New(StacktraceObject, &StacktraceTypeObject);
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

    if (PyObject_TypeCheck(this->crashthread, &ThreadTypeObject))
    {
        bo->crashthread = (ThreadObject *)p_btp_gdb_thread_dup((PyObject *)this->crashthread, PyTuple_New(0));
        if (!bo->crashthread)
            return NULL;
    }
    else
        bo->crashthread = (ThreadObject *)Py_None;

    if (PyObject_TypeCheck(this->crashframe, &FrameTypeObject))
    {
        bo->crashframe = (FrameObject *)p_btp_gdb_thread_dup((PyObject *)this->crashframe, PyTuple_New(0));
        if (!bo->crashframe)
            return NULL;
    }
    else
        bo->crashframe = (FrameObject *)Py_None;

    return (PyObject *)bo;
}

PyObject *p_btp_gdb_stacktrace_find_crash_frame(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys linked list - need to rebuild python list */
    struct btp_gdb_frame *frame = btp_gdb_stacktrace_get_crash_frame(this->stacktrace);
    if (!frame)
    {
        PyErr_SetString(PyExc_LookupError, "Crash frame not found");
        return NULL;
    }

    FrameObject *result = (FrameObject *)PyObject_New(FrameObject, &FrameTypeObject);
    if (!result)
        return PyErr_NoMemory();

    result->frame = frame;
    this->crashframe = result;

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject *)result;
}

PyObject *p_btp_gdb_stacktrace_find_crash_thread(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys linked list - need to rebuild python list */
    struct btp_gdb_thread *thread = btp_gdb_stacktrace_find_crash_thread(this->stacktrace);
    if (!thread)
    {
        PyErr_SetString(PyExc_LookupError, "Crash thread not found");
        return NULL;
    }

    ThreadObject *result = (ThreadObject *)PyObject_New(ThreadObject, &ThreadTypeObject);
    if (!result)
        return PyErr_NoMemory();

    result->thread = btp_gdb_thread_dup(thread, false);
    result->frames = frame_linked_list_to_python_list(result->thread);
    this->crashthread = result;

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject *)result;
}

PyObject *p_btp_gdb_stacktrace_limit_frame_depth(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
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

PyObject *p_btp_gdb_stacktrace_quality_simple(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = btp_gdb_stacktrace_quality_simple(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *p_btp_gdb_stacktrace_quality_complex(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    float result = btp_gdb_stacktrace_quality_complex(this->stacktrace);
    return Py_BuildValue("f", result);
}

PyObject *p_btp_gdb_stacktrace_get_duplication_hash(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    char *duphash = btp_gdb_stacktrace_get_duplication_hash(this->stacktrace);
    PyObject *result = Py_BuildValue("s", duphash);
    free(duphash);

    return result;
}

PyObject *p_btp_gdb_stacktrace_find_address(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    unsigned long long address;
    if (!PyArg_ParseTuple(args, "l", &address))
        return NULL;

    if (address == -1)
        Py_RETURN_NONE;

    int i;
    SharedlibObject *item;
    for (i = 0; i < PyList_Size(this->libs); ++i)
    {
        item = (SharedlibObject *)PyList_GetItem(this->libs, i);
        if (!item)
            return NULL;

        if (item->sharedlib->from <= address && item->sharedlib->to >= address)
        {
            Py_INCREF(item);
            return (PyObject *)item;
        }
    }

    Py_RETURN_NONE;
}

PyObject *p_btp_gdb_stacktrace_set_libnames(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* does not destroy the linked list */
    btp_gdb_stacktrace_set_libnames(this->stacktrace);
    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyObject *p_btp_gdb_stacktrace_normalize(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    /* destroys the linked list and frees some parts */
    /* need to rebuild python list manually */
    struct btp_gdb_stacktrace *tmp = btp_gdb_stacktrace_dup(this->stacktrace);
    btp_normalize_stacktrace(tmp);
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

PyObject *p_btp_gdb_stacktrace_get_optimized_thread(PyObject *self, PyObject *args)
{
    StacktraceObject *this = (StacktraceObject *)self;
    if (stacktrace_prepare_linked_list(this) < 0)
        return NULL;

    int max_frames;
    if (!PyArg_ParseTuple(args, "i", &max_frames))
        return NULL;

    struct btp_gdb_thread *thread = btp_gdb_stacktrace_get_optimized_thread(this->stacktrace, max_frames);
    if (!thread)
    {
        PyErr_SetString(PyExc_LookupError, "Crash thread not found");
        return NULL;
    }

    ThreadObject *result = (ThreadObject *)PyObject_New(ThreadObject, &ThreadTypeObject);
    if (!result)
        return PyErr_NoMemory();

    result->thread = thread;
    result->frames = frame_linked_list_to_python_list(result->thread);

    if (stacktrace_rebuild_thread_python_list(this) < 0)
        return NULL;

    return (PyObject *)result;
}
