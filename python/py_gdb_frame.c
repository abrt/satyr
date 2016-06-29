/*
    py_gdb_frame.c

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
#include "py_common.h"
#include "py_gdb_frame.h"
#include "py_base_frame.h"

#include <inttypes.h>

#include "location.h"
#include "strbuf.h"
#include "utils.h"
#include "gdb/frame.h"

#define frame_doc "satyr.GdbFrame - class representing a frame in a thread\n\n" \
                  "Usage:\n\n" \
                  "satyr.GdbFrame() - creates an empty frame\n\n" \
                  "satyr.GdbFrame(str) - parses str and fills the frame object"

#define f_dup_doc "Usage: frame.dup()\n\n" \
                  "Returns: satyr.GdbFrame - a new clone of the frame\n\n" \
                  "Clones the frame object. All new structures are independent " \
                  "of the original object."

#define f_calls_func_doc "Usage: frame.calls_func(name)\n\n" \
                         "name: string - function name\n\n" \
                         "Returns: integer - 0 = False, 1 = True\n\n" \
                         "Checks whether the frame represents a call to " \
                         "a function with given name."

#define f_calls_func_in_file_doc "Usage: frame.calls_func_in_file(name, filename)\n\n" \
                                 "name: string - function name\n\n" \
                                 "filename: string - file name\n\n" \
                                 "Returns: integer - 0 = False, 1 = True\n\n" \
                                 "Checks whether the frame represents a call to " \
                                 "a function with given name from a given file."


static PyMethodDef
frame_methods[] =
{
    /* methods */
    { "dup",                sr_py_gdb_frame_dup,                METH_NOARGS,  f_dup_doc                },
    { "calls_func",         sr_py_gdb_frame_calls_func,         METH_VARARGS, f_calls_func_doc         },
    { "calls_func_in_file", sr_py_gdb_frame_calls_func_in_file, METH_VARARGS, f_calls_func_in_file_doc },
    { NULL },
};

/*
 * This sequence of #defines and macros expands to definition of struct
 * getset_offsets, one for each member to be accessed with the generic
 * getters/setters.
 *
 * Python objects for GDB frames store data in struct sr_py_gdb_frame, which
 * contains pointer named frame that points to the GDB frame C structure named
 * struct sr_gdb_frame. Most of the satyr python object follow similar scheme,
 * which allows us to use generic getters/setters that only need to know two
 * offsets.
 */

/* GSOFF_PY_STRUCT must be defined to the type of the python object structure (without the struct keyword). */
#define GSOFF_PY_STRUCT sr_py_gdb_frame

/* GSOFF_PY_MEMBER must be defined to name of GSOFF_PY_STRUCT's member that points to the satyr C structure. */
#define GSOFF_PY_MEMBER frame

/* GSOFF_C_STRUCT must be defined to the type of the C structure (without the struct keyword) */
#define GSOFF_C_STRUCT sr_gdb_frame

/*
 * Individual GSOFF_MEMBER invocations need to be surrounded by GSOFF_START and
 * GSOFF_END and need to be separated by commas (which must not be after the
 * last invocation). The order does not matter.
 *
 * E.g. the GSOFF_MEMBER(function_name) expands to definition of global
 * variable gsoff_function_name containing the right offsets, which is then
 * used int the frame_getset[] definition below.
 */
GSOFF_START
GSOFF_MEMBER(function_name),
GSOFF_MEMBER(function_type),
GSOFF_MEMBER(source_file),
GSOFF_MEMBER(number),
GSOFF_MEMBER(source_line),
GSOFF_MEMBER(signal_handler_called),
GSOFF_MEMBER(address),
GSOFF_MEMBER(library_name)
GSOFF_END

/*
 * With the GSOFF_ declarations above, we can use SR_ATTRIBUTE_ macros that
 * expand to PyGetSetDef literals with the right values. There is also
 * SR_ATTRIBUTE_type_R variant that allows you to name the attribute different
 * than the C struct member.
 */
static PyGetSetDef
frame_getset[] =
{
    SR_ATTRIBUTE_STRING(function_name,         "Function name (string)"                    ),
    SR_ATTRIBUTE_STRING(function_type,         "Function type (string)"                    ),
    SR_ATTRIBUTE_UINT32(number,                "Frame number (positive integer)"           ),
    SR_ATTRIBUTE_STRING(source_file,           "Source file name (string)"                 ),
    SR_ATTRIBUTE_UINT32(source_line,           "Source line number (positive integer)"     ),
    SR_ATTRIBUTE_BOOL  (signal_handler_called, "True if the frame is signal handler (bool)"),
    SR_ATTRIBUTE_UINT64(address,               "Address of the current instruction (long)" ),
    SR_ATTRIBUTE_STRING(library_name,          "Executable file name (string)"             ),
    { NULL },
};

PyTypeObject
sr_py_gdb_frame_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.GdbFrame",           /* tp_name */
    sizeof(struct sr_py_gdb_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_gdb_frame_free,       /* tp_dealloc */
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
    sr_py_gdb_frame_str,        /* tp_str */
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
    frame_methods,              /* tp_methods */
    NULL,                       /* tp_members */
    frame_getset,               /* tp_getset */
    &sr_py_base_frame_type,     /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_gdb_frame_new,        /* tp_new */
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
sr_py_gdb_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_gdb_frame *fo = (struct sr_py_gdb_frame*)
        PyObject_New(struct sr_py_gdb_frame, &sr_py_gdb_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct sr_location location;
        sr_location_init(&location);
        fo->frame = sr_gdb_frame_parse(&str, &location);

        if (!fo->frame)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        fo->frame = sr_gdb_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
sr_py_gdb_frame_free(PyObject *object)
{
    struct sr_py_gdb_frame *this = (struct sr_py_gdb_frame*)object;
    sr_gdb_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_gdb_frame_str(PyObject *self)
{
    struct sr_py_gdb_frame *this = (struct sr_py_gdb_frame*)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "Frame #%u: ", this->frame->number);
    if (!this->frame->function_name)
        sr_strbuf_append_str(buf, "signal handler");
    else if (strncmp("??", this->frame->function_name, strlen("??")) == 0)
        sr_strbuf_append_str(buf, "unknown function");
    else
        sr_strbuf_append_strf(buf, "function %s", this->frame->function_name);
    if (this->frame->address != (sr_py_gdb_frame_address_t) -1)
        sr_strbuf_append_strf(buf, " @ 0x%016"PRIx64, this->frame->address);
    if (this->frame->library_name)
        sr_strbuf_append_strf(buf, " (%s)", this->frame->library_name);
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* methods */
PyObject *
sr_py_gdb_frame_dup(PyObject *self, PyObject *args)
{
    struct sr_py_gdb_frame *this = (struct sr_py_gdb_frame*)self;
    struct sr_py_gdb_frame *fo = (struct sr_py_gdb_frame*)
        PyObject_New(struct sr_py_gdb_frame, &sr_py_gdb_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = sr_gdb_frame_dup(this->frame, false);

    return (PyObject*)fo;
}

PyObject *
sr_py_gdb_frame_calls_func(PyObject *self, PyObject *args)
{
    char *func_name;
    if (!PyArg_ParseTuple(args, "s", &func_name))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    return Py_BuildValue("i", sr_gdb_frame_calls_func(frame, func_name, NULL));
}

PyObject *
sr_py_gdb_frame_calls_func_in_file(PyObject *self, PyObject *args)
{
    char *func_name, *file_name;
    if (!PyArg_ParseTuple(args, "ss", &func_name, &file_name))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    return Py_BuildValue("i", sr_gdb_frame_calls_func(frame, func_name, file_name, NULL));
}
