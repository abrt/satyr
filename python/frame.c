#include "common.h"

static PyMethodDef FrameMethods[] = {
    /* getters & setters */
    { "get_function_name",         p_btp_frame_get_function_name,         METH_NOARGS,  f_get_function_name_doc         },
    { "set_function_name",         p_btp_frame_set_function_name,         METH_VARARGS, f_set_function_name_doc         },
    { "get_function_type",         p_btp_frame_get_function_type,         METH_NOARGS,  f_get_function_type_doc         },
    { "set_function_type",         p_btp_frame_set_function_type,         METH_VARARGS, f_set_function_type_doc         },
    { "get_number",                p_btp_frame_get_number,                METH_NOARGS,  f_get_number_doc                },
    { "set_number",                p_btp_frame_set_number,                METH_VARARGS, f_set_number_doc                },
    { "get_source_file",           p_btp_frame_get_source_file,           METH_NOARGS,  f_get_source_file_doc           },
    { "set_source_file",           p_btp_frame_set_source_file,           METH_VARARGS, f_set_source_file_doc           },
    { "get_source_line",           p_btp_frame_get_source_line,           METH_NOARGS,  f_get_source_line_doc           },
    { "set_source_line",           p_btp_frame_set_source_line,           METH_VARARGS, f_set_source_line_doc           },
    { "get_signal_handler_called", p_btp_frame_get_signal_handler_called, METH_NOARGS,  f_get_signal_handler_called_doc },
    { "set_signal_handler_called", p_btp_frame_set_signal_handler_called, METH_VARARGS, f_set_signal_handler_called_doc },
    { "get_address",               p_btp_frame_get_address,               METH_NOARGS,  f_get_address_doc               },
    { "set_address",               p_btp_frame_set_address,               METH_VARARGS, f_set_address_doc               },
    { "get_library_name",          p_btp_frame_get_library_name,          METH_NOARGS,  f_get_library_name_doc          },
    { "set_library_name",          p_btp_frame_set_library_name,          METH_VARARGS, f_set_library_name_doc          },
    /* methods */
    { "dup",                       p_btp_frame_dup,                       METH_NOARGS,  f_dup_doc                       },
    { "cmp",                       p_btp_frame_cmp,                       METH_VARARGS, f_cmp_doc                       },
    { "calls_func",                p_btp_frame_calls_func,                METH_VARARGS, f_calls_func_doc                },
    { "calls_func_in_file",        p_btp_frame_calls_func_in_file,        METH_VARARGS, f_calls_func_in_file_doc        },
    { NULL },
};

PyTypeObject FrameTypeObject = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Frame",           /* tp_name */
    sizeof(FrameObject),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    p_btp_frame_free,           /* tp_dealloc */
    NULL,                       /* tp_print */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    NULL,                       /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    p_btp_frame_str,            /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    frame_doc,                  /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    FrameMethods,               /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    p_btp_frame_new,            /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* constructor */
PyObject *p_btp_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    FrameObject *fo = (FrameObject *)PyObject_New(FrameObject, &FrameTypeObject);
    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct btp_location location;
        btp_location_init(&location);
        fo->frame = btp_frame_parse(&str, &location);

        if (!fo->frame)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        fo->frame = btp_frame_new();

    return (PyObject *)fo;
}

/* destructor */
void p_btp_frame_free(PyObject *object)
{
    FrameObject *this = (FrameObject *)object;
    btp_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *p_btp_frame_str(PyObject *self)
{
    FrameObject *this = (FrameObject *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Frame #%u: ", this->frame->number);
    if (!this->frame->function_name)
        btp_strbuf_append_str(buf, "signal handler");
    else if (strncmp("??", this->frame->function_name, strlen("??")) == 0)
        btp_strbuf_append_str(buf, "unknown function");
    else
        btp_strbuf_append_strf(buf, "function %s", this->frame->function_name);
    if (this->frame->address != -1)
        btp_strbuf_append_strf(buf, " @ 0x%016lx", this->frame->address);
    if (this->frame->library_name)
        btp_strbuf_append_strf(buf, " (%s)", this->frame->library_name);
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

/* function_name */
PyObject *p_btp_frame_get_function_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((FrameObject *)self)->frame->function_name);
}

PyObject *p_btp_frame_set_function_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    free(frame->function_name);
    frame->function_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* function_type */
PyObject *p_btp_frame_get_function_type(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((FrameObject *)self)->frame->function_type);
}

PyObject *p_btp_frame_set_function_type(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    free(frame->function_type);
    frame->function_type = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* number */
PyObject *p_btp_frame_get_number(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((FrameObject *)self)->frame->number);
}

PyObject *p_btp_frame_set_number(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Frame number must not be negative.");
        return NULL;
    }

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    frame->number = newvalue;
    Py_RETURN_NONE;
}

/* source_file */
PyObject *p_btp_frame_get_source_file(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((FrameObject *)self)->frame->source_file);
}

PyObject *p_btp_frame_set_source_file(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    free(frame->source_file);
    frame->source_file = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* source_line */
PyObject *p_btp_frame_get_source_line(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((FrameObject *)self)->frame->source_line);
}

PyObject *p_btp_frame_set_source_line(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Source line must not be negative.");
        return NULL;
    }

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    frame->source_line = newvalue;
    Py_RETURN_NONE;
}

/* signal_handler_called */
PyObject *p_btp_frame_get_signal_handler_called(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((FrameObject *)self)->frame->signal_handler_called);
}

PyObject *p_btp_frame_set_signal_handler_called(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    frame->signal_handler_called = newvalue ? 1 : 0;
    Py_RETURN_NONE;
}

/* address */
PyObject *p_btp_frame_get_address(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((FrameObject *)self)->frame->address);
}

PyObject *p_btp_frame_set_address(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    frame->address = newvalue;
    Py_RETURN_NONE;
}

/* library_name */
PyObject *p_btp_frame_get_library_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((FrameObject *)self)->frame->library_name);
}

PyObject *p_btp_frame_set_library_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    free(frame->library_name);
    frame->library_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}


/* methods */
PyObject *p_btp_frame_dup(PyObject *self, PyObject *args)
{
    FrameObject *this = (FrameObject *)self;
    FrameObject *fo = (FrameObject *)PyObject_New(FrameObject, &FrameTypeObject);
    if (!fo)
        return PyErr_NoMemory();
    fo->frame = btp_frame_dup(this->frame, false);

    return (PyObject *)fo;
}

PyObject *p_btp_frame_cmp(PyObject *self, PyObject *args)
{
    PyObject *compare_to;
    int compare_number;
    if (!PyArg_ParseTuple(args, "O!i", &FrameTypeObject, &compare_to, &compare_number))
        return NULL;

    struct btp_frame *f1 = ((FrameObject *)self)->frame;
    struct btp_frame *f2 = ((FrameObject *)compare_to)->frame;

    return Py_BuildValue("i", btp_frame_cmp(f1, f2, compare_number));
}

PyObject *p_btp_frame_calls_func(PyObject *self, PyObject *args)
{
    char *func_name;
    if (!PyArg_ParseTuple(args, "s", &func_name))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    return Py_BuildValue("i", btp_frame_calls_func(frame, func_name));
}

PyObject *p_btp_frame_calls_func_in_file(PyObject *self, PyObject *args)
{
    char *func_name, *file_name;
    if (!PyArg_ParseTuple(args, "ss", &func_name, &file_name))
        return NULL;

    struct btp_frame *frame = ((FrameObject *)self)->frame;
    return Py_BuildValue("i", btp_frame_calls_func_in_file(frame, func_name, file_name));
}
