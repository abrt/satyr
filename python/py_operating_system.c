/* py_operating_system.c Copyright (C) 2013  Red Hat, Inc.  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or
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
#include "py_operating_system.h"

#include "operating_system.h"
#include "utils.h"
#include <glib.h>

#define operating_system_doc "satyr.OperatingSystem - describes an operating system\n\n" \
                             "Usage:\n\n" \
                             "satyr.OperatingSystem() - creates an empty operating system object\n\n" \
                             "satyr.OperatingSystem(name, version, arch) - creates an operating system\n" \
                             "object with given properties (all arguments are strings or None)"

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_operating_system
#define GSOFF_PY_MEMBER operating_system
#define GSOFF_C_STRUCT sr_operating_system
GSOFF_START
GSOFF_MEMBER(name),
GSOFF_MEMBER(version),
GSOFF_MEMBER(architecture),
GSOFF_MEMBER(cpe),
GSOFF_MEMBER(uptime)
GSOFF_END

static PyGetSetDef
operating_system_getset[] =
{
    SR_ATTRIBUTE_STRING(name,         "Operating system name (string)"                  ),
    SR_ATTRIBUTE_STRING(version,      "Version (string)"                                ),
    SR_ATTRIBUTE_STRING(architecture, "Architecture (string)"                           ),
    SR_ATTRIBUTE_STRING(cpe,          "Common platform enumeration identifier (string)" ),
    SR_ATTRIBUTE_UINT64(uptime,       "Machine uptime (long)"                           ),
    { NULL },
};

PyTypeObject
sr_py_operating_system_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.OperatingSystem",    /* tp_name */
    sizeof(struct sr_py_operating_system), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_operating_system_free,/* tp_dealloc */
    0,                          /* tp_vectorcall_offset */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    NULL,                       /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    sr_py_operating_system_str, /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    operating_system_doc,       /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    NULL,                       /* tp_methods */
    NULL,                       /* tp_members */
    operating_system_getset,    /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_operating_system_new, /* tp_new */
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
sr_py_operating_system_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_operating_system *os =
        PyObject_New(struct sr_py_operating_system, &sr_py_operating_system_type);

    if (!os)
        return PyErr_NoMemory();

    os->operating_system = sr_operating_system_new();

    const char *name = NULL, *version = NULL, *arch = NULL;
    if (!PyArg_ParseTuple(args, "|sss", &name, &version, &arch))
        return NULL;

    if (name)
        os->operating_system->name = g_strdup(name);

    if (version)
        os->operating_system->version = g_strdup(version);

    if (arch)
        os->operating_system->architecture = g_strdup(arch);

    return (PyObject*)os;
}

/* destructor */
void
sr_py_operating_system_free(PyObject *object)
{
    struct sr_py_operating_system *this = (struct sr_py_operating_system *)object;
    sr_operating_system_free(this->operating_system);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_operating_system_str(PyObject *object)
{
    struct sr_py_operating_system *this = (struct sr_py_operating_system*)object;
    GString *buf = g_string_new(NULL);

    g_string_append(buf,
        this->operating_system->name ? this->operating_system->name : "(unknown)");

    if (this->operating_system->version)
        g_string_append_printf(buf, " %s", this->operating_system->version);

    if (this->operating_system->architecture)
        g_string_append_printf(buf, " (%s)", this->operating_system->architecture);

    if (this->operating_system->cpe)
        g_string_append_printf(buf, ", CPE: %s", this->operating_system->cpe);

    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}
