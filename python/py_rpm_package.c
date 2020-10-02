/*
    py_rpm_package.c

    Copyright (C) 2013  Red Hat, Inc.

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
#include "py_rpm_package.h"

#include "rpm.h"
#include "utils.h"
#include <glib.h>

#define rpm_package_doc "satyr.RpmPackage - RPM package representation\n\n" \
                        "Usage:\n\n" \
                        "satyr.RpmPackage() - creates an empty RPM package object\n\n" \
                        "satyr.RpmPackage(name, epoch, version, release, arch) - creates RPM package\n" \
                        "object with given properties"

#define rpm_role_doc "Role the package plays in the problem. Currently ROLE_UNKNOWN or ROLE_AFFECTED."

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_rpm_package
#define GSOFF_PY_MEMBER rpm_package
#define GSOFF_C_STRUCT sr_rpm_package
GSOFF_START
GSOFF_MEMBER(name),
GSOFF_MEMBER(epoch),
GSOFF_MEMBER(version),
GSOFF_MEMBER(release),
GSOFF_MEMBER(architecture),
GSOFF_MEMBER(install_time)
/* GSOFF_MEMBER(role) */
GSOFF_END

static PyGetSetDef
rpm_package_getset[] =
{
    SR_ATTRIBUTE_STRING(name,         "Package name (string)"       ),
    SR_ATTRIBUTE_UINT32(epoch,        "Epoch (int)"                 ),
    SR_ATTRIBUTE_STRING(version,      "Version (string)"            ),
    SR_ATTRIBUTE_STRING(release,      "Release (string)"            ),
    SR_ATTRIBUTE_STRING(architecture, "Architecture (string)"       ),
    SR_ATTRIBUTE_UINT64(install_time, "Time of installation (long)" ),
    { (char*)"role", sr_py_rpm_package_get_role, sr_py_rpm_package_set_role, (char*)rpm_role_doc, NULL },
    { NULL },
};

PyTypeObject
sr_py_rpm_package_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.RpmPackage",         /* tp_name */
    sizeof(struct sr_py_rpm_package), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_rpm_package_free,     /* tp_dealloc */
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
    sr_py_rpm_package_str,      /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    rpm_package_doc,            /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    NULL,                       /* tp_methods */
    NULL,                       /* tp_members */
    rpm_package_getset,         /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_rpm_package_new,      /* tp_new */
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
sr_py_rpm_package_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_rpm_package *rpm =
        PyObject_New(struct sr_py_rpm_package, &sr_py_rpm_package_type);

    if (!rpm)
        return PyErr_NoMemory();

    rpm->rpm_package = sr_rpm_package_new();

    unsigned int epoch;
    const char *name = NULL, *version = NULL, *rel = NULL, *arch = NULL;
    if (!PyArg_ParseTuple(args, "|sIsss", &name, &epoch, &version, &rel, &arch))
        return NULL;

    if (name)
        rpm->rpm_package->name = g_strdup(name);

    if (rel)
        rpm->rpm_package->release = g_strdup(rel);

    if (version)
        rpm->rpm_package->version = g_strdup(version);

    if (arch)
        rpm->rpm_package->architecture = g_strdup(arch);

    rpm->rpm_package->epoch = (uint32_t)epoch;

    return (PyObject*)rpm;
}

/* destructor */
void
sr_py_rpm_package_free(PyObject *object)
{
    struct sr_py_rpm_package *this = (struct sr_py_rpm_package *)object;
    sr_rpm_package_free(this->rpm_package, false);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_rpm_package_str(PyObject *object)
{
    struct sr_py_rpm_package *this = (struct sr_py_rpm_package*)object;
    GString *buf = g_string_new(NULL);

    if (this->rpm_package->name)
    {
        g_string_append(buf, this->rpm_package->name);

        if (this->rpm_package->version)
        {
            g_string_append(buf, "-");

            if (this->rpm_package->epoch)
                g_string_append_printf(buf, "%u:", (unsigned)this->rpm_package->epoch);

            g_string_append(buf, this->rpm_package->version);

            if (this->rpm_package->release)
            {
                g_string_append_printf(buf, "-%s", this->rpm_package->release);

                if (this->rpm_package->architecture)
                    g_string_append_printf(buf, ".%s", this->rpm_package->architecture);
            }
        }

    }
    else
        g_string_append(buf, "(unknown)");

    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */

PyObject *
sr_py_rpm_package_get_role(PyObject *self, void *data)
{
    return Py_BuildValue("i", ((struct sr_py_rpm_package*)self)->rpm_package->role);
}

int
sr_py_rpm_package_set_role(PyObject *self, PyObject *rhs, void *data)
{
    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    long newvalue = PyInt_AsLong(rhs);
    if (PyErr_Occurred())
        return -1;

    if (newvalue != SR_ROLE_UNKNOWN && newvalue != SR_ROLE_AFFECTED)
    {
        PyErr_SetString(PyExc_ValueError, "Role must be either ROLE_UNKNOWN "
                                          "or ROLE_AFFECTED.");
        return -1;
    }

    ((struct sr_py_rpm_package *)self)->rpm_package->role = newvalue;
    return 0;
}
