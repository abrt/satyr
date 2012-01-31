#include "common.h"

static PyMethodDef SharedlibMethods[] = {
    /* getters & setters */
    { "get_from",    p_btp_sharedlib_get_from,    METH_NOARGS,  s_get_from_doc    },
    { "set_from",    p_btp_sharedlib_set_from,    METH_VARARGS, s_set_from_doc    },
    { "get_to",      p_btp_sharedlib_get_to,      METH_NOARGS,  s_get_to_doc      },
    { "set_to",      p_btp_sharedlib_set_to,      METH_VARARGS, s_set_to_doc      },
    { "get_symbols", p_btp_sharedlib_get_symbols, METH_NOARGS,  s_get_symbols_doc },
    { "set_symbols", p_btp_sharedlib_set_symbols, METH_VARARGS, s_set_symbols_doc },
    { "get_soname",  p_btp_sharedlib_get_soname,  METH_NOARGS,  s_get_soname_doc  },
    { "set_soname",  p_btp_sharedlib_set_soname,  METH_VARARGS, s_set_soname_doc  },
    /* methods */
    { NULL },
};

static PyMemberDef SharedlibMembers[] = {
    { (char *)"SYMS_OK",        T_INT, offsetof(SharedlibObject, syms_ok),        READONLY, s_syms_ok_doc        },
    { (char *)"SYMS_WRONG",     T_INT, offsetof(SharedlibObject, syms_wrong),     READONLY, s_syms_wrong_doc     },
    { (char *)"SYMS_NOT_FOUND", T_INT, offsetof(SharedlibObject, syms_not_found), READONLY, s_syms_not_found_doc },
    { NULL },
};

PyTypeObject SharedlibTypeObject = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Sharedlib",       /* tp_name */
    sizeof(SharedlibObject),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    p_btp_sharedlib_free,       /* tp_dealloc */
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
    p_btp_sharedlib_str,        /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    sharedlib_doc,              /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    SharedlibMethods,           /* tp_methods */
    SharedlibMembers,           /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    p_btp_sharedlib_new,        /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* constructor */
PyObject *p_btp_sharedlib_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    SharedlibObject *so = (SharedlibObject *)PyObject_New(SharedlibObject, &SharedlibTypeObject);
    if (!so)
        return PyErr_NoMemory();

    so->sharedlib = btp_sharedlib_new();
    so->syms_ok = SYMS_OK;
    so->syms_wrong = SYMS_WRONG;
    so->syms_not_found = SYMS_NOT_FOUND;

    return (PyObject *)so;
}

/* destructor */
void p_btp_sharedlib_free(PyObject *object)
{
    SharedlibObject *this = (SharedlibObject *)object;
    btp_sharedlib_free(this->sharedlib);
    PyObject_Del(object);
}

/* str */
PyObject *p_btp_sharedlib_str(PyObject *self)
{
    return Py_BuildValue("s", ((SharedlibObject *)self)->sharedlib->soname ?: "Unknown shared library");
}

/* getters & setters */

/* function_name */
PyObject *p_btp_sharedlib_get_from(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((SharedlibObject *)self)->sharedlib->from);
}

PyObject *p_btp_sharedlib_set_from(PyObject *self, PyObject *args)
{
    unsigned long long newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    ((SharedlibObject *)self)->sharedlib->from = newvalue;
    Py_RETURN_NONE;
}

PyObject *p_btp_sharedlib_get_to(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((SharedlibObject *)self)->sharedlib->to);
}

PyObject *p_btp_sharedlib_set_to(PyObject *self, PyObject *args)
{
    unsigned long long newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    ((SharedlibObject *)self)->sharedlib->to = newvalue;
    Py_RETURN_NONE;
}

PyObject *p_btp_sharedlib_get_symbols(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((SharedlibObject *)self)->sharedlib->symbols);
}

PyObject *p_btp_sharedlib_set_symbols(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue != SYMS_OK && newvalue != SYMS_WRONG && newvalue != SYMS_NOT_FOUND)
    {
        PyErr_SetString(PyExc_ValueError, "Symbols must be either SYMS_OK, "
                                          "SYMS_WRONG or SYMS_NOT_FOUND.");
        return NULL;
    }

    ((SharedlibObject *)self)->sharedlib->symbols = newvalue;
    Py_RETURN_NONE;
}

PyObject *p_btp_sharedlib_get_soname(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((SharedlibObject *)self)->sharedlib->soname);
}

PyObject *p_btp_sharedlib_set_soname(PyObject *self, PyObject *args)
{
    char *soname;
    if (!PyArg_ParseTuple(args, "s", &soname))
        return NULL;

    SharedlibObject *this = (SharedlibObject *)self;
    free(this->sharedlib->soname);
    this->sharedlib->soname = btp_strdup(soname);
    Py_RETURN_NONE;
}
