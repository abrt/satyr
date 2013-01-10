#include "py_gdb_sharedlib.h"
#include "lib/gdb_sharedlib.h"
#include "lib/utils.h"

#define sharedlib_doc "btparser.GdbSharedlib - class representing a shared library loaded at the moment of crash"

#define s_get_from_doc "Usage: sharedlib.get_from()\n" \
                       "Returns: long - address where lib's memory begins"

#define s_set_from_doc "Usage: sharedlib.set_from(newvalue)\n" \
                       "Newvalue: long - address where lib's memory begins"

#define s_get_to_doc "Usage: sharedlib.get_to()\n" \
                     "Returns: long - address where lib's memory ends"

#define s_set_to_doc "Usage: sharedlib.set_to(newvalue)\n" \
                     "Newvalue: long - address where lib's memory ends"

#define s_get_symbols_doc "Usage: sharedlib.get_symbols()\n" \
                          "Returns: SYMS_OK / SYMS_NOT_FOUND / SYMS_WRONG"

#define s_set_symbols_doc "Usage: sharedlib.set_symbols(newvalue)\n" \
                          "Newvalue: SYMS_OK / SYMS_NOT_FOUND / SYMS_WRONG"

#define s_get_soname_doc "Usage: sharedlib.get_soname()\n" \
                         "Returns: string - library filename"

#define s_set_soname_doc "Usage: sharedlib.set_soname(newvalue)\n" \
                         "Newvalue: string - library filename"

#define s_syms_ok_doc (char *)"Constant. Debug symbols for the library were loaded successfully."

#define s_syms_wrong_doc (char *)"Constant. Debug symbols for the library were present, but did not match."

#define s_syms_not_found_doc (char *)"Constant. Debug symbols for the library were not found."

static PyMethodDef
gdb_sharedlib_methods[] =
{
    /* getters & setters */
    { "get_from",    btp_py_gdb_sharedlib_get_from,    METH_NOARGS,  s_get_from_doc    },
    { "set_from",    btp_py_gdb_sharedlib_set_from,    METH_VARARGS, s_set_from_doc    },
    { "get_to",      btp_py_gdb_sharedlib_get_to,      METH_NOARGS,  s_get_to_doc      },
    { "set_to",      btp_py_gdb_sharedlib_set_to,      METH_VARARGS, s_set_to_doc      },
    { "get_symbols", btp_py_gdb_sharedlib_get_symbols, METH_NOARGS,  s_get_symbols_doc },
    { "set_symbols", btp_py_gdb_sharedlib_set_symbols, METH_VARARGS, s_set_symbols_doc },
    { "get_soname",  btp_py_gdb_sharedlib_get_soname,  METH_NOARGS,  s_get_soname_doc  },
    { "set_soname",  btp_py_gdb_sharedlib_set_soname,  METH_VARARGS, s_set_soname_doc  },
    /* methods */
    { NULL },
};

static PyMemberDef
gdb_sharedlib_members[] =
{
    { (char *)"SYMS_OK",        T_INT, offsetof(struct btp_py_gdb_sharedlib, syms_ok),        READONLY, s_syms_ok_doc        },
    { (char *)"SYMS_WRONG",     T_INT, offsetof(struct btp_py_gdb_sharedlib, syms_wrong),     READONLY, s_syms_wrong_doc     },
    { (char *)"SYMS_NOT_FOUND", T_INT, offsetof(struct btp_py_gdb_sharedlib, syms_not_found), READONLY, s_syms_not_found_doc },
    { NULL },
};

PyTypeObject
btp_py_gdb_sharedlib_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.GdbSharedlib",       /* tp_name */
    sizeof(struct btp_py_gdb_sharedlib),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    btp_py_gdb_sharedlib_free,   /* tp_dealloc */
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
    btp_py_gdb_sharedlib_str,    /* tp_str */
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
    gdb_sharedlib_methods,      /* tp_methods */
    gdb_sharedlib_members,      /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    btp_py_gdb_sharedlib_new,    /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* constructor */
PyObject *
btp_py_gdb_sharedlib_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct btp_py_gdb_sharedlib *so = (struct btp_py_gdb_sharedlib*)
        PyObject_New(struct btp_py_gdb_sharedlib,
                     &btp_py_gdb_sharedlib_type);

    if (!so)
        return PyErr_NoMemory();

    so->sharedlib = btp_gdb_sharedlib_new();
    so->syms_ok = SYMS_OK;
    so->syms_wrong = SYMS_WRONG;
    so->syms_not_found = SYMS_NOT_FOUND;

    return (PyObject *)so;
}

/* destructor */
void
btp_py_gdb_sharedlib_free(PyObject *object)
{
    struct btp_py_gdb_sharedlib *this = (struct btp_py_gdb_sharedlib*)object;
    btp_gdb_sharedlib_free(this->sharedlib);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_gdb_sharedlib_str(PyObject *self)
{
    return Py_BuildValue("s", ((struct btp_py_gdb_sharedlib*)self)->sharedlib->soname ?: "Unknown shared library");
}

/* getters & setters */

/* function_name */
PyObject *
btp_py_gdb_sharedlib_get_from(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_gdb_sharedlib*)self)->sharedlib->from);
}

PyObject *
btp_py_gdb_sharedlib_set_from(PyObject *self, PyObject *args)
{
    unsigned long long newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    ((struct btp_py_gdb_sharedlib *)self)->sharedlib->from = newvalue;
    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_sharedlib_get_to(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_gdb_sharedlib*)self)->sharedlib->to);
}

PyObject *
btp_py_gdb_sharedlib_set_to(PyObject *self, PyObject *args)
{
    unsigned long long newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    ((struct btp_py_gdb_sharedlib *)self)->sharedlib->to = newvalue;
    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_sharedlib_get_symbols(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct btp_py_gdb_sharedlib*)self)->sharedlib->symbols);
}

PyObject *
btp_py_gdb_sharedlib_set_symbols(PyObject *self, PyObject *args)
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

    ((struct btp_py_gdb_sharedlib *)self)->sharedlib->symbols = newvalue;
    Py_RETURN_NONE;
}

PyObject *
btp_py_gdb_sharedlib_get_soname(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_gdb_sharedlib*)self)->sharedlib->soname);
}

PyObject *
btp_py_gdb_sharedlib_set_soname(PyObject *self, PyObject *args)
{
    char *soname;
    if (!PyArg_ParseTuple(args, "s", &soname))
        return NULL;

    struct btp_py_gdb_sharedlib *this = (struct btp_py_gdb_sharedlib*)self;
    free(this->sharedlib->soname);
    this->sharedlib->soname = btp_strdup(soname);
    Py_RETURN_NONE;
}
