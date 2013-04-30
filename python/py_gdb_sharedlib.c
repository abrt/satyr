#include "py_common.h"
#include "py_gdb_sharedlib.h"
#include "lib/gdb_sharedlib.h"
#include "lib/utils.h"

#define sharedlib_doc "satyr.GdbSharedlib - class representing a shared library loaded at the moment of crash"

#define s_symbols_doc "Symbol state (satyr.SYMS_OK / satyr.SYMS_NOT_FOUND / satyr.SYMS_WRONG)\n"         \
                      "SYMS_OK:        Debug symbols for the library were loaded successfully.\n"        \
                      "SYMS_WRONG:     Debug symbols for the library were present, but did not match.\n" \
                      "SYMS_NOT_FOUND: Debug symbols for the library were not found."

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_gdb_sharedlib
#define GSOFF_PY_MEMBER sharedlib
#define GSOFF_C_STRUCT sr_gdb_sharedlib
GSOFF_START
GSOFF_MEMBER(from),
GSOFF_MEMBER(to),
GSOFF_MEMBER(soname)
GSOFF_END

static PyGetSetDef
gdb_sharedlib_getset[] =
{
    SR_ATTRIBUTE_UINT64_R("start_address", from, "Address where lib's memory begins (long)"),
    SR_ATTRIBUTE_UINT64_R("end_address",   to,   "Address where lib's memory ends (long)"  ),
    SR_ATTRIBUTE_STRING(soname, "Library file name (string)"),
    { (char*)"symbols", sr_py_gdb_sharedlib_get_symbols, sr_py_gdb_sharedlib_set_symbols, (char*)s_symbols_doc, NULL },
    { NULL },
};

PyTypeObject
sr_py_gdb_sharedlib_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.GdbSharedlib",       /* tp_name */
    sizeof(struct sr_py_gdb_sharedlib),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_gdb_sharedlib_free,   /* tp_dealloc */
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
    sr_py_gdb_sharedlib_str,    /* tp_str */
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
    NULL,                       /* tp_methods */
    NULL,                       /* tp_members */
    gdb_sharedlib_getset,       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_gdb_sharedlib_new,    /* tp_new */
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
sr_py_gdb_sharedlib_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_gdb_sharedlib *so = (struct sr_py_gdb_sharedlib*)
        PyObject_New(struct sr_py_gdb_sharedlib,
                     &sr_py_gdb_sharedlib_type);

    if (!so)
        return PyErr_NoMemory();

    so->sharedlib = sr_gdb_sharedlib_new();

    return (PyObject *)so;
}

/* destructor */
void
sr_py_gdb_sharedlib_free(PyObject *object)
{
    struct sr_py_gdb_sharedlib *this = (struct sr_py_gdb_sharedlib*)object;
    sr_gdb_sharedlib_free(this->sharedlib);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_gdb_sharedlib_str(PyObject *self)
{
    return Py_BuildValue("s", ((struct sr_py_gdb_sharedlib*)self)->sharedlib->soname ?: "Unknown shared library");
}

/* getters & setters */

PyObject *
sr_py_gdb_sharedlib_get_symbols(PyObject *self, void *data)
{
    return Py_BuildValue("i", ((struct sr_py_gdb_sharedlib*)self)->sharedlib->symbols);
}

int
sr_py_gdb_sharedlib_set_symbols(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    long newvalue = PyInt_AsLong(rhs);
    if (PyErr_Occurred())
        return -1;

    if (newvalue != SYMS_OK && newvalue != SYMS_WRONG && newvalue != SYMS_NOT_FOUND)
    {
        PyErr_SetString(PyExc_ValueError, "Symbols must be either SYMS_OK, "
                                          "SYMS_WRONG or SYMS_NOT_FOUND.");
        return -1;
    }

    ((struct sr_py_gdb_sharedlib *)self)->sharedlib->symbols = newvalue;
    return 0;
}
