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
#include "py_gdb_frame.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/utils.h"
#include "lib/gdb_frame.h"

#define frame_doc "satyr.GdbFrame - class representing a frame in a thread\n" \
                  "Usage:\n" \
                  "satyr.GdbFrame() - creates an empty frame\n" \
                  "satyr.GdbFrame(str) - parses str and fills the frame object"

#define f_get_function_name_doc "Usage: frame.get_function_name()\n" \
                                "Returns: string - function name"

#define f_set_function_name_doc "Usage: frame.set_function_name(newname)\n" \
                                "newname: string - new function name"

#define f_get_function_type_doc "Usage: frame.get_function_type()\n" \
                                "Returns: string - function type"

#define f_set_function_type_doc "Usage: frame.set_function_type(newtype)\n" \
                                "newtype: string - new function type"

#define f_get_number_doc "Usage: frame.get_number()\n" \
                         "Returns: positive integer - frame number"

#define f_set_number_doc "Usage: frame.set_number(N)\n" \
                         "N: positive integer - new frame number"

#define f_get_source_line_doc "Usage: frame.get_source_line()\n" \
                              "Returns: positive integer - source line number"

#define f_set_source_line_doc "Usage: frame.set_source_line(N)\n" \
                              "N: positive integer - new source line number"

#define f_get_source_file_doc "Usage: frame.get_source_file()\n" \
                              "Returns: string - source file name"

#define f_set_source_file_doc "Usage: frame.set_source_file(newname)\n" \
                              "newname: string - new source file name"

#define f_get_signal_handler_called_doc "Usage: frame.get_signal_handler_called()\n" \
                                        "Returns: integer - 0 = False, 1 = True"

#define f_set_signal_handler_called_doc "Usage: frame.set_signal_handler_called(N)\n" \
                                        "N: integer - 0 = False, anything else = True"

#define f_get_address_doc "Usage: frame.get_address()\n" \
                          "Returns: positive long integer - address"

#define f_set_address_doc "Usage: frame.set_address(N)\n" \
                          "N: positive long integer - new address"

#define f_get_library_name_doc "Usage: frame.get_library_name()\n" \
                              "Returns: string - library name"

#define f_set_library_name_doc "Usage: frame.set_library_name(newname)\n" \
                              "newname: string - new library name"

#define f_dup_doc "Usage: frame.dup()\n" \
                  "Returns: satyr.GdbFrame - a new clone of frame\n" \
                  "Clones the frame object. All new structures are independent " \
                  "on the original object."

#define f_calls_func_doc "Usage: frame.calls_func(name)\n" \
                         "name: string - function name\n" \
                         "Returns: integer - 0 = False, 1 = True\n" \
                         "Checks whether the frame represents a call to " \
                         "a function with given name."

#define f_calls_func_in_file_doc "Usage: frame.calls_func_in_file(name, filename)\n" \
                                 "name: string - function name\n" \
                                 "filename: string - file name\n" \
                                 "Returns: integer - 0 = False, 1 = True\n" \
                                 "Checks whether the frame represents a call to " \
                                 "a function with given name from a given file."

#define f_cmp_doc "Usage: frame.cmp(frame2, compare_number)\n" \
                  "frame2: satyr.Frame - another frame to compare\n" \
                  "compare_number: boolean - whether to compare also thread numbers\n" \
                  "Returns: integer - distance\n" \
                  "Compares frame to frame2. Returns 0 if frame = frame2, " \
                  "<0 if frame is 'less' than frame2, >0 if frame is 'more' " \
                  "than frame2."


static PyMethodDef
frame_methods[] =
{
    /* getters & setters */
    { "get_function_name",         sr_py_gdb_frame_get_function_name,         METH_NOARGS,  f_get_function_name_doc         },
    { "set_function_name",         sr_py_gdb_frame_set_function_name,         METH_VARARGS, f_set_function_name_doc         },
    { "get_function_type",         sr_py_gdb_frame_get_function_type,         METH_NOARGS,  f_get_function_type_doc         },
    { "set_function_type",         sr_py_gdb_frame_set_function_type,         METH_VARARGS, f_set_function_type_doc         },
    { "get_number",                sr_py_gdb_frame_get_number,                METH_NOARGS,  f_get_number_doc                },
    { "set_number",                sr_py_gdb_frame_set_number,                METH_VARARGS, f_set_number_doc                },
    { "get_source_file",           sr_py_gdb_frame_get_source_file,           METH_NOARGS,  f_get_source_file_doc           },
    { "set_source_file",           sr_py_gdb_frame_set_source_file,           METH_VARARGS, f_set_source_file_doc           },
    { "get_source_line",           sr_py_gdb_frame_get_source_line,           METH_NOARGS,  f_get_source_line_doc           },
    { "set_source_line",           sr_py_gdb_frame_set_source_line,           METH_VARARGS, f_set_source_line_doc           },
    { "get_signal_handler_called", sr_py_gdb_frame_get_signal_handler_called, METH_NOARGS,  f_get_signal_handler_called_doc },
    { "set_signal_handler_called", sr_py_gdb_frame_set_signal_handler_called, METH_VARARGS, f_set_signal_handler_called_doc },
    { "get_address",               sr_py_gdb_frame_get_address,               METH_NOARGS,  f_get_address_doc               },
    { "set_address",               sr_py_gdb_frame_set_address,               METH_VARARGS, f_set_address_doc               },
    { "get_library_name",          sr_py_gdb_frame_get_library_name,          METH_NOARGS,  f_get_library_name_doc          },
    { "set_library_name",          sr_py_gdb_frame_set_library_name,          METH_VARARGS, f_set_library_name_doc          },
    /* methods */
    { "dup",                       sr_py_gdb_frame_dup,                       METH_NOARGS,  f_dup_doc                       },
    { "cmp",                       sr_py_gdb_frame_cmp,                       METH_VARARGS, f_cmp_doc                       },
    { "calls_func",                sr_py_gdb_frame_calls_func,                METH_VARARGS, f_calls_func_doc                },
    { "calls_func_in_file",        sr_py_gdb_frame_calls_func_in_file,        METH_VARARGS, f_calls_func_in_file_doc        },
    { NULL },
};

PyTypeObject
sr_py_gdb_frame_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
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
    if (this->frame->address != -1)
        sr_strbuf_append_strf(buf, " @ 0x%016"PRIx64, this->frame->address);
    if (this->frame->library_name)
        sr_strbuf_append_strf(buf, " (%s)", this->frame->library_name);
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

/* function_name */
PyObject *
sr_py_gdb_frame_get_function_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct sr_py_gdb_frame*)self)->frame->function_name);
}

PyObject *
sr_py_gdb_frame_set_function_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    free(frame->function_name);
    frame->function_name = sr_strdup(newvalue);
    Py_RETURN_NONE;
}

/* function_type */
PyObject *
sr_py_gdb_frame_get_function_type(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct sr_py_gdb_frame*)self)->frame->function_type);
}

PyObject *
sr_py_gdb_frame_set_function_type(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    free(frame->function_type);
    frame->function_type = sr_strdup(newvalue);
    Py_RETURN_NONE;
}

/* number */
PyObject *
sr_py_gdb_frame_get_number(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct sr_py_gdb_frame*)self)->frame->number);
}

PyObject *
sr_py_gdb_frame_set_number(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Frame number must not be negative.");
        return NULL;
    }

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    frame->number = newvalue;
    Py_RETURN_NONE;
}

/* source_file */
PyObject *
sr_py_gdb_frame_get_source_file(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct sr_py_gdb_frame*)self)->frame->source_file);
}

PyObject *
sr_py_gdb_frame_set_source_file(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    free(frame->source_file);
    frame->source_file = sr_strdup(newvalue);
    Py_RETURN_NONE;
}

/* source_line */
PyObject *
sr_py_gdb_frame_get_source_line(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct sr_py_gdb_frame*)self)->frame->source_line);
}

PyObject *
sr_py_gdb_frame_set_source_line(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Source line must not be negative.");
        return NULL;
    }

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    frame->source_line = newvalue;
    Py_RETURN_NONE;
}

/* signal_handler_called */
PyObject *
sr_py_gdb_frame_get_signal_handler_called(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct sr_py_gdb_frame*)self)->frame->signal_handler_called);
}

PyObject *
sr_py_gdb_frame_set_signal_handler_called(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    frame->signal_handler_called = newvalue ? 1 : 0;
    Py_RETURN_NONE;
}

/* address */
PyObject *
sr_py_gdb_frame_get_address(PyObject *self, PyObject *args)
{
    return Py_BuildValue("K",
        (unsigned long long)(((struct sr_py_gdb_frame*)self)->frame->address));
}

PyObject *
sr_py_gdb_frame_set_address(PyObject *self, PyObject *args)
{
    unsigned long long newvalue;
    if (!PyArg_ParseTuple(args, "K", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    frame->address = newvalue;
    Py_RETURN_NONE;
}

/* library_name */
PyObject *
sr_py_gdb_frame_get_library_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct sr_py_gdb_frame*)self)->frame->library_name);
}

PyObject *
sr_py_gdb_frame_set_library_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    free(frame->library_name);
    frame->library_name = sr_strdup(newvalue);
    Py_RETURN_NONE;
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
sr_py_gdb_frame_cmp(PyObject *self, PyObject *args)
{
    PyObject *compare_to;
    int compare_number;
    if (!PyArg_ParseTuple(args,
                          "O!i",
                          &sr_py_gdb_frame_type,
                          &compare_to,
                          &compare_number))
    {
        return NULL;
    }

    struct sr_gdb_frame *f1 = ((struct sr_py_gdb_frame*)self)->frame;
    struct sr_gdb_frame *f2 = ((struct sr_py_gdb_frame*)compare_to)->frame;
    return Py_BuildValue("i", sr_gdb_frame_cmp(f1, f2, compare_number));
}

PyObject *
sr_py_gdb_frame_calls_func(PyObject *self, PyObject *args)
{
    char *func_name;
    if (!PyArg_ParseTuple(args, "s", &func_name))
        return NULL;

    struct sr_gdb_frame *frame = ((struct sr_py_gdb_frame*)self)->frame;
    return Py_BuildValue("i", sr_gdb_frame_calls_func(frame, func_name));
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
