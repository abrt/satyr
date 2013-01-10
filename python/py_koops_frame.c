/*
    py_koops_frame.c

    Copyright (C) 2010, 2011, 2012  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "py_koops_frame.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/utils.h"
#include "lib/koops_frame.h"

#define frame_doc "btparser.KerneloopsFrame - class representing a frame in a kerneloops\n" \
                  "Usage:\n" \
                  "btparser.KerneloopsFrame() - creates an empty frame\n" \
                  "btparser.KerneloopsFrame(str) - parses str and fills the frame object"

#define f_get_reliable_doc "Usage: frame.get_reliable()\n" \
                          "Returns: boolean - is this entry reliable"

#define f_set_reliable_doc "Usage: frame.set_address(state)\n" \
                          "state: boolean - set reliability to state"

#define f_get_address_doc "Usage: frame.get_address()\n" \
                          "Returns: positive long integer - address"

#define f_set_address_doc "Usage: frame.set_address(N)\n" \
                          "N: positive long integer - new address"

#define f_get_function_name_doc "Usage: frame.get_function_name()\n" \
                                "Returns: string - function name"

#define f_set_function_name_doc "Usage: frame.set_function_name(newname)\n" \
                                "newname: string - new function name"

#define f_get_function_offset_doc "Usage: frame.get_function_offset()\n" \
                                "Returns: positive long integer - function offset"

#define f_set_function_offset_doc "Usage: frame.set_function_offset(newoffset)\n" \
                                "newoffset: positive long integer - new function offset"

#define f_get_function_length_doc "Usage: frame.get_function_length()\n" \
                                "Returns: positive long integer - function length"

#define f_set_function_length_doc "Usage: frame.set_function_length(newlength)\n" \
                                "newlength: positive long integer - new function length"

#define f_get_module_name_doc "Usage: frame.get_module_name()\n" \
                                "Returns: string - module name"

#define f_set_module_name_doc "Usage: frame.set_module_name(newname)\n" \
                                "newname: string - new module name"



#define f_get_from_address_doc "Usage: frame.get_from_address()\n" \
                          "Returns: positive long integer - from address"

#define f_set_from_address_doc "Usage: frame.set_from_address(N)\n" \
                          "N: positive long integer - new from address"

#define f_get_from_function_name_doc "Usage: frame.get_from_function_name()\n" \
                                "Returns: string - from function name"

#define f_set_from_function_name_doc "Usage: frame.set_from_function_name(newname)\n" \
                                "newname: string - new from function name"

#define f_get_from_function_offset_doc "Usage: frame.get_from_function_offset()\n" \
                                "Returns: positive long integer - from function offset"

#define f_set_from_function_offset_doc "Usage: frame.set_from_function_offset(newoffset)\n" \
                                "newoffset: positive long integer - new from function offset"

#define f_get_from_function_length_doc "Usage: frame.get_from_function_length()\n" \
                                "Returns: positive long integer - from function length"

#define f_set_from_function_length_doc "Usage: frame.set_from_function_length(newlength)\n" \
                                "newlength: positive long integer - new from function length"

#define f_get_from_module_name_doc "Usage: frame.get_from_module_name()\n" \
                                "Returns: string - from module name"

#define f_set_from_module_name_doc "Usage: frame.set_from_module_name(newname)\n" \
                                "newname: string - new from module name"


#define f_dup_doc "Usage: frame.dup()\n" \
                  "Returns: btparser.KerneloopsFrame - a new clone of frame\n" \
                  "Clones the frame object. All new structures are independent " \
                  "on the original object."

#define f_cmp_doc "Usage: frame.cmp(frame2, compare_number)\n" \
                  "frame2: btparser.KerneloopsFrame - another frame to compare\n" \
                  "Returns: integer - distance\n" \
                  "Compares frame to frame2. Returns 0 if frame = frame2, " \
                  "<0 if frame is 'less' than frame2, >0 if frame is 'more' " \
                  "than frame2."


static PyMethodDef
frame_methods[] =
{
    /* getters & setters */
    { "get_reliable",              btp_py_koops_frame_get_reliable,             METH_NOARGS,  f_get_reliable_doc               },
    { "set_reliable",              btp_py_koops_frame_set_reliable,             METH_VARARGS, f_set_reliable_doc               },
    { "get_address",               btp_py_koops_frame_get_address,              METH_NOARGS,  f_get_address_doc                },
    { "set_address",               btp_py_koops_frame_set_address,              METH_VARARGS, f_set_address_doc                },
    { "get_function_name",         btp_py_koops_frame_get_function_name,        METH_NOARGS,  f_get_function_name_doc          },
    { "set_function_name",         btp_py_koops_frame_set_function_name,        METH_VARARGS, f_set_function_name_doc          },
    { "get_function_offset",       btp_py_koops_frame_get_function_offset,      METH_NOARGS,  f_get_function_offset_doc        },
    { "set_function_offset",       btp_py_koops_frame_set_function_offset,      METH_VARARGS, f_set_function_offset_doc        },
    { "get_function_length",       btp_py_koops_frame_get_function_length,      METH_NOARGS,  f_get_function_length_doc        },
    { "set_function_length",       btp_py_koops_frame_set_function_length,      METH_VARARGS, f_set_function_length_doc        },
    { "get_module_name",           btp_py_koops_frame_get_module_name,          METH_NOARGS,  f_get_module_name_doc            },
    { "set_module_name",           btp_py_koops_frame_set_module_name,          METH_VARARGS, f_set_module_name_doc            },
    { "get_from_address",          btp_py_koops_frame_get_from_address,         METH_NOARGS,  f_get_from_address_doc           },
    { "set_from_address",          btp_py_koops_frame_set_from_address,         METH_VARARGS, f_set_from_address_doc           },
    { "get_from_function_name",    btp_py_koops_frame_get_from_function_name,   METH_NOARGS,  f_get_from_function_name_doc     },
    { "set_from_function_name",    btp_py_koops_frame_set_from_function_name,   METH_VARARGS, f_set_from_function_name_doc     },
    { "get_from_function_offset",  btp_py_koops_frame_get_from_function_offset, METH_NOARGS,  f_get_from_function_offset_doc   },
    { "set_from_function_offset",  btp_py_koops_frame_set_from_function_offset, METH_VARARGS, f_set_from_function_offset_doc   },
    { "get_from_function_length",  btp_py_koops_frame_get_from_function_length, METH_NOARGS,  f_get_from_function_length_doc   },
    { "set_from_function_length",  btp_py_koops_frame_set_from_function_length, METH_VARARGS, f_set_from_function_length_doc   },
    { "get_from_module_name",      btp_py_koops_frame_get_from_module_name,     METH_NOARGS,  f_get_from_module_name_doc       },
    { "set_from_module_name",      btp_py_koops_frame_set_from_module_name,     METH_VARARGS, f_set_from_module_name_doc       },

    /* methods */
    { "dup",                       btp_py_koops_frame_dup,                      METH_NOARGS,  f_dup_doc                        },
    { "cmp",                       btp_py_koops_frame_cmp,                      METH_VARARGS, f_cmp_doc                        },
    { NULL },
};

PyTypeObject
btp_py_koops_frame_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.KerneloopsFrame",   /* tp_name */
    sizeof(struct btp_py_koops_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    btp_py_koops_frame_free,       /* tp_dealloc */
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
    btp_py_koops_frame_str,        /* tp_str */
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
    frame_methods,               /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    btp_py_koops_frame_new,        /* tp_new */
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
btp_py_koops_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct btp_py_koops_frame *fo = (struct btp_py_koops_frame*)
        PyObject_New(struct btp_py_koops_frame, &btp_py_koops_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        fo->frame = btp_koops_frame_parse(&str);
    }
    else
        fo->frame = btp_koops_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
btp_py_koops_frame_free(PyObject *object)
{
    struct btp_py_koops_frame *this = (struct btp_py_koops_frame*)object;
    btp_koops_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_koops_frame_str(PyObject *self)
{
    struct btp_py_koops_frame *this = (struct btp_py_koops_frame*)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    if (this->frame->address != 0)
        btp_strbuf_append_strf(buf, "[0x%016lx] ", this->frame->address);

    if (!this->frame->reliable)
        btp_strbuf_append_str(buf, "? ");

    if (this->frame->function_name)
        btp_strbuf_append_str(buf, this->frame->function_name);

    if (this->frame->function_offset)
        btp_strbuf_append_strf(buf, "+0x%lx", this->frame->function_offset);

    if (this->frame->function_length)
        btp_strbuf_append_strf(buf, "/0x%lx", this->frame->function_length);

    if (this->frame->module_name)
        btp_strbuf_append_strf(buf, " [%s]", this->frame->module_name);

    if (this->frame->from_function_name || this->frame->from_address)
        btp_strbuf_append_str(buf, " from ");

    if (this->frame->from_address != 0)
        btp_strbuf_append_strf(buf, "[0x%016lx] ", this->frame->from_address);

    if (this->frame->from_function_name)
        btp_strbuf_append_str(buf, this->frame->from_function_name);

    if (this->frame->from_function_offset)
        btp_strbuf_append_strf(buf, "+0x%lx", this->frame->from_function_offset);

    if (this->frame->from_function_length)
        btp_strbuf_append_strf(buf, "/0x%lx", this->frame->from_function_length);

    if (this->frame->from_module_name)
        btp_strbuf_append_strf(buf, " [%s]", this->frame->from_module_name);

    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

/* reliable */
PyObject *
btp_py_koops_frame_get_reliable(PyObject *self, PyObject *args)
{
    if (!((struct btp_py_koops_frame*)self)->frame->reliable)
       Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}

PyObject *
btp_py_koops_frame_set_reliable(PyObject *self, PyObject *args)
{
    int boolvalue;
    if (!PyArg_ParseTuple(args, "i", &boolvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->reliable = boolvalue;
    Py_RETURN_NONE;
}

/* address */
PyObject *
btp_py_koops_frame_get_address(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->address);
}

PyObject *
btp_py_koops_frame_set_address(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->address = newvalue;
    Py_RETURN_NONE;
}

/* from_address */
PyObject *
btp_py_koops_frame_get_from_address(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->from_address);
}

PyObject *
btp_py_koops_frame_set_from_address(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->from_address = newvalue;
    Py_RETURN_NONE;
}


/* function_name */
PyObject *
btp_py_koops_frame_get_function_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_koops_frame*)self)->frame->function_name);
}

PyObject *
btp_py_koops_frame_set_function_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    free(frame->function_name);
    frame->function_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* from_function_name */
PyObject *
btp_py_koops_frame_get_from_function_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_koops_frame*)self)->frame->from_function_name);
}

PyObject *
btp_py_koops_frame_set_from_function_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    free(frame->function_name);
    frame->from_function_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* function_offset */
PyObject *
btp_py_koops_frame_get_function_offset(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->function_offset);
}

PyObject *
btp_py_koops_frame_set_function_offset(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->function_offset = newvalue;
    Py_RETURN_NONE;
}

/* from_function_offset */
PyObject *
btp_py_koops_frame_get_from_function_offset(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->from_function_offset);
}

PyObject *
btp_py_koops_frame_set_from_function_offset(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->from_function_offset = newvalue;
    Py_RETURN_NONE;
}

/* function_length */
PyObject *
btp_py_koops_frame_get_function_length(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->function_length);
}

PyObject *
btp_py_koops_frame_set_function_length(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->function_length = newvalue;
    Py_RETURN_NONE;
}

/* from_function_length */
PyObject *
btp_py_koops_frame_get_from_function_length(PyObject *self, PyObject *args)
{
    return Py_BuildValue("l", ((struct btp_py_koops_frame*)self)->frame->from_function_length);
}

PyObject *
btp_py_koops_frame_set_from_function_length(PyObject *self, PyObject *args)
{
    uint64_t newvalue;
    if (!PyArg_ParseTuple(args, "l", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    frame->from_function_length = newvalue;
    Py_RETURN_NONE;
}

/* module_name */
PyObject *
btp_py_koops_frame_get_module_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_koops_frame*)self)->frame->module_name);
}

PyObject *
btp_py_koops_frame_set_module_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    free(frame->module_name);
    frame->module_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* from_module_name */
PyObject *
btp_py_koops_frame_get_from_module_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_koops_frame*)self)->frame->from_module_name);
}

PyObject *
btp_py_koops_frame_set_from_module_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_koops_frame *frame = ((struct btp_py_koops_frame*)self)->frame;
    free(frame->module_name);
    frame->from_module_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* methods */
PyObject *
btp_py_koops_frame_dup(PyObject *self, PyObject *args)
{
    struct btp_py_koops_frame *this = (struct btp_py_koops_frame*)self;
    struct btp_py_koops_frame *fo = (struct btp_py_koops_frame*)
        PyObject_New(struct btp_py_koops_frame, &btp_py_koops_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = btp_koops_frame_dup(this->frame, false);

    return (PyObject*)fo;
}

PyObject *
btp_py_koops_frame_cmp(PyObject *self, PyObject *args)
{
    PyObject *compare_to;
    if (!PyArg_ParseTuple(args,
                          "O!i",
                          &btp_py_koops_frame_type,
                          &compare_to))
    {
        return NULL;
    }

    struct btp_koops_frame *f1 = ((struct btp_py_koops_frame*)self)->frame;
    struct btp_koops_frame *f2 = ((struct btp_py_koops_frame*)compare_to)->frame;
    return Py_BuildValue("i", btp_koops_frame_cmp(f1, f2));
}
