/*
    py_python_frame.c

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
#include "py_python_frame.h"
#include "lib/location.h"
#include "lib/strbuf.h"
#include "lib/utils.h"
#include "lib/python_frame.h"

#define frame_doc "btparser.PythonFrame - class representing a python frame\n" \
                  "Usage:\n" \
                  "btparser.PythonFrame() - creates an empty frame\n" \
                  "btparser.PythonFrame(str) - parses str and fills the frame object"

#define f_get_file_name_doc "Usage: frame.get_file_name()\n" \
                                "Returns: string - file name"

#define f_set_file_name_doc "Usage: frame.set_file_name(newname)\n" \
                                "newname: string - new file name"

#define f_get_file_line_doc "Usage: frame.get_file_line()\n" \
                                "Returns: unsigned int - file line"

#define f_set_file_line_doc "Usage: frame.set_file_line(newline)\n" \
                                "newline: unsigned int - new file line"

#define f_is_module_doc "Usage: frame.is_module()\n" \
                          "Returns: boolean - is this entry from main module"

#define f_set_is_module_doc "Usage: frame.is_modules(state)\n" \
                          "state: boolean - set is_module to state"

#define f_get_function_name_doc "Usage: frame.get_function_name()\n" \
                                "Returns: string - function name"

#define f_set_function_name_doc "Usage: frame.set_function_name(newname)\n" \
                                "newname: string - new function name"

#define f_get_line_contents_doc "Usage: frame.get_line_contents()\n" \
                                "Returns: string - remaining line contents"

#define f_set_line_contents_doc "Usage: frame.set_line_contents(newname)\n" \
                                "newname: string - new line contents"

#define f_dup_doc "Usage: frame.dup()\n" \
                  "Returns: btparser.PythonFrame - a new clone of frame\n" \
                  "Clones the frame object. All new structures are independent " \
                  "on the original object."

#define f_cmp_doc "Usage: frame.cmp(frame2)\n" \
                  "frame2: btparser.Frame - another frame to compare\n" \
                  "Returns: integer - distance\n" \
                  "Compares frame to frame2. Returns 0 if frame = frame2, " \
                  "<0 if frame is 'less' than frame2, >0 if frame is 'more' " \
                  "than frame2."


static PyMethodDef
frame_methods[] =
{
    /* getters & setters */
    { "get_file_name",             btp_py_python_frame_get_file_name,         METH_NOARGS,      f_get_file_name_doc             },
    { "set_file_name",             btp_py_python_frame_set_file_name,         METH_VARARGS,     f_set_file_name_doc             },
    { "get_file_line",             btp_py_python_frame_get_file_line,         METH_NOARGS,      f_get_file_line_doc             },
    { "set_file_line",             btp_py_python_frame_set_file_line,         METH_VARARGS,     f_set_file_line_doc             },
    { "is_module",                 btp_py_python_frame_is_module,             METH_NOARGS,      f_is_module_doc                 },
    { "set_is_module",             btp_py_python_frame_set_is_module,         METH_VARARGS,     f_set_is_module_doc             },
    { "get_function_name",         btp_py_python_frame_get_function_name,     METH_NOARGS,      f_get_function_name_doc         },
    { "set_function_name",         btp_py_python_frame_set_function_name,     METH_VARARGS,     f_set_function_name_doc         },
    { "get_line_contents",         btp_py_python_frame_get_line_contents,     METH_NOARGS,      f_get_line_contents_doc         },
    { "set_line_contents",         btp_py_python_frame_set_line_contents,     METH_VARARGS,     f_set_line_contents_doc         },
    /* methods */
    { "dup",                       btp_py_python_frame_dup,                   METH_NOARGS,      f_dup_doc                       },
    { "cmp",                       btp_py_python_frame_cmp,                   METH_VARARGS,     f_cmp_doc                       },
    { NULL },
};

PyTypeObject
btp_py_python_frame_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.PythonFrame",     /* tp_name */
    sizeof(struct btp_py_python_frame),        /* tp_basicsize */
    0,                          /* tp_itemsize */
    btp_py_python_frame_free,       /* tp_dealloc */
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
    btp_py_python_frame_str,    /* tp_str */
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
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    btp_py_python_frame_new,    /* tp_new */
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
btp_py_python_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct btp_py_python_frame *fo = (struct btp_py_python_frame*)
        PyObject_New(struct btp_py_python_frame, &btp_py_python_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    const char *str = NULL;
    if (!PyArg_ParseTuple(args, "|s", &str))
        return NULL;

    if (str)
    {
        struct btp_location location;
        btp_location_init(&location);
        fo->frame = btp_python_frame_parse(&str, &location);

        if (!fo->frame)
        {
            PyErr_SetString(PyExc_ValueError, location.message);
            return NULL;
        }
    }
    else
        fo->frame = btp_python_frame_new();

    return (PyObject*)fo;
}

/* destructor */
void
btp_py_python_frame_free(PyObject *object)
{
    struct btp_py_python_frame *this = (struct btp_py_python_frame*)object;
    btp_python_frame_free(this->frame);
    PyObject_Del(object);
}

/* str */
PyObject *
btp_py_python_frame_str(PyObject *self)
{
    struct btp_py_python_frame *this = (struct btp_py_python_frame*)self;
    struct btp_strbuf *buf = btp_strbuf_new();


    if (this->frame->file_name)
      btp_strbuf_append_strf(buf, "File \"%s\"", this->frame->file_name);

    if (this->frame->file_line)
      btp_strbuf_append_strf(buf, ", %d", this->frame->file_line);

    if (this->frame->function_name)
      btp_strbuf_append_strf(buf, ", in %s", this->frame->function_name);

    if (this->frame->is_module)
      btp_strbuf_append_str(buf, ", in <module>");

    if (this->frame->line_contents)
      btp_strbuf_append_strf(buf, "\n    %s", this->frame->line_contents);

    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

/* file_name */
PyObject *
btp_py_python_frame_get_file_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_python_frame*)self)->frame->file_name);
}

PyObject *
btp_py_python_frame_set_file_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_python_frame *frame = ((struct btp_py_python_frame*)self)->frame;
    free(frame->file_name);
    frame->file_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* file_line */
PyObject *
btp_py_python_frame_get_file_line(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", ((struct btp_py_python_frame*)self)->frame->file_line);
}

PyObject *
btp_py_python_frame_set_file_line(PyObject *self, PyObject *args)
{
    int newvalue;
    if (!PyArg_ParseTuple(args, "i", &newvalue))
        return NULL;

    if (newvalue < 0)
    {
        PyErr_SetString(PyExc_ValueError, "File line must not be negative.");
        return NULL;
    }

    struct btp_python_frame *frame = ((struct btp_py_python_frame*)self)->frame;
    frame->file_line = newvalue;
    Py_RETURN_NONE;
}

/* reliable */
PyObject *
btp_py_python_frame_is_module(PyObject *self, PyObject *args)
{
    if (!((struct btp_py_python_frame*)self)->frame->is_module)
       Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}

PyObject *
btp_py_python_frame_set_is_module(PyObject *self, PyObject *args)
{
    int boolvalue;
    if (!PyArg_ParseTuple(args, "i", &boolvalue))
        return NULL;

    struct btp_python_frame *frame = ((struct btp_py_python_frame*)self)->frame;
    frame->is_module = boolvalue;
    Py_RETURN_NONE;
}

/* function_name */
PyObject *
btp_py_python_frame_get_function_name(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_python_frame*)self)->frame->function_name);
}

PyObject *
btp_py_python_frame_set_function_name(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_python_frame *frame = ((struct btp_py_python_frame*)self)->frame;
    free(frame->function_name);
    frame->function_name = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* line_contents */
PyObject *
btp_py_python_frame_get_line_contents(PyObject *self, PyObject *args)
{
    return Py_BuildValue("s", ((struct btp_py_python_frame*)self)->frame->line_contents);
}

PyObject *
btp_py_python_frame_set_line_contents(PyObject *self, PyObject *args)
{
    char *newvalue;
    if (!PyArg_ParseTuple(args, "s", &newvalue))
        return NULL;

    struct btp_python_frame *frame = ((struct btp_py_python_frame*)self)->frame;
    free(frame->line_contents);
    frame->line_contents = btp_strdup(newvalue);
    Py_RETURN_NONE;
}

/* methods */
PyObject *
btp_py_python_frame_dup(PyObject *self, PyObject *args)
{
    struct btp_py_python_frame *this = (struct btp_py_python_frame*)self;
    struct btp_py_python_frame *fo = (struct btp_py_python_frame*)
        PyObject_New(struct btp_py_python_frame, &btp_py_python_frame_type);

    if (!fo)
        return PyErr_NoMemory();

    fo->frame = btp_python_frame_dup(this->frame, false);

    return (PyObject*)fo;
}

PyObject *
btp_py_python_frame_cmp(PyObject *self, PyObject *args)
{
    PyObject *compare_to;
    if (!PyArg_ParseTuple(args,
                          "O!",
                          &btp_py_python_frame_type,
                          &compare_to))
    {
        return NULL;
    }

    struct btp_python_frame *f1 = ((struct btp_py_python_frame*)self)->frame;
    struct btp_python_frame *f2 = ((struct btp_py_python_frame*)compare_to)->frame;
    return Py_BuildValue("i", btp_python_frame_cmp(f1, f2));
}
