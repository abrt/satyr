/*
    py_report.c

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
#include "py_report.h"
#include "py_rpm_package.h"
#include "py_operating_system.h"

#include "py_base_stacktrace.h"
#include "py_core_stacktrace.h"
#include "py_python_stacktrace.h"
#include "py_koops_stacktrace.h"
#include "py_java_stacktrace.h"

#include "report.h"
#include "operating_system.h"
#include "strbuf.h"
#include "rpm.h"

#define report_doc "satyr.Report - report containing all data relevant to a software problem\n" \
                   "Usage:\n" \
                   "satyr.Report() - creates an empty report object\n" \
                   "satyr.Report(json_string) - creates a report from its JSON representation"

#define to_json_doc "Usage: report.to_json()\n" \
                    "Returns: string - the report serialized as JSON"

/* See python/py_common.h and python/py_gdb_frame.c for generic getters/setters documentation. */
#define GSOFF_PY_STRUCT sr_py_report
#define GSOFF_PY_MEMBER report
#define GSOFF_C_STRUCT sr_report
GSOFF_START
GSOFF_MEMBER(reporter_name),
GSOFF_MEMBER(reporter_version),
GSOFF_MEMBER(user_root),
GSOFF_MEMBER(user_local),
GSOFF_MEMBER(component_name)
GSOFF_END

static PyGetSetDef
report_getset[] =
{
    SR_ATTRIBUTE_STRING(reporter_name,    "Name of the reporting software (string)"                               ),
    SR_ATTRIBUTE_STRING(reporter_version, "Version of the reporting software (string)"                            ),
    SR_ATTRIBUTE_BOOL(user_root,          "Did the problem originate in a program running as root? (bool)"        ),
    SR_ATTRIBUTE_BOOL(user_local,         "Did the problem originate in a program executed by local user? (bool)" ),
    SR_ATTRIBUTE_STRING(component_name,   "Name of the software component this report pertains to (string)"       ),
    { (char*)"report_version", sr_py_report_get_version, sr_py_setter_readonly, (char*)"Version of the report (int)", NULL },
    { (char*)"report_type", sr_py_report_get_type, sr_py_report_set_type, (char*)"Report type (string)", NULL },
    { NULL },
};

static PyMemberDef
report_members[] =
{
    { (char*)"stacktrace", T_OBJECT_EX, offsetof(struct sr_py_report, stacktrace), 0, (char*)"Problem stacktrace" },
    { (char*)"operating_system", T_OBJECT_EX, offsetof(struct sr_py_report, operating_system), 0, (char *)"Operating system" },
    { (char*)"packages", T_OBJECT_EX, offsetof(struct sr_py_report, packages), 0, (char *)"List of packages" },
    { NULL },
};

static PyMethodDef
report_methods[] =
{
    { "to_json", sr_py_report_to_json, METH_NOARGS, to_json_doc },
    { NULL },
};

PyTypeObject
sr_py_report_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.Report",             /* tp_name */
    sizeof(struct sr_py_report), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_report_free,          /* tp_dealloc */
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
    sr_py_report_str,           /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    report_doc,                 /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    report_methods,             /* tp_methods */
    report_members,             /* tp_members */
    report_getset,              /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_report_new,           /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

static PyObject *
rpms_to_python_list(struct sr_rpm_package *rpm)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return PyErr_NoMemory();

    struct sr_py_rpm_package *item;
    while (rpm)
    {
        item = PyObject_New(struct sr_py_rpm_package, &sr_py_rpm_package_type);
        if (!item)
            return PyErr_NoMemory();

        item->rpm_package = rpm;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        rpm = rpm->next;
    }

    return result;
}

int
rpms_prepare_linked_list(struct sr_py_report *report)
{
    if (!PyList_Check(report->packages))
    {
        PyErr_SetString(PyExc_TypeError, "Attribute 'packages' is not a list.");
        return -1;
    }

    int i;
    PyObject *item;
    struct sr_py_rpm_package *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(report->packages); ++i)
    {
        item = PyList_GetItem(report->packages, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, &sr_py_rpm_package_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError, "packages must be a list of RpmPackage objects");
            return -1;
        }

        current = (struct sr_py_rpm_package*)item;
        if (i == 0)
            report->report->rpm_packages = current->rpm_package;
        else
            prev->rpm_package->next = current->rpm_package;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->rpm_package->next = NULL;
        Py_DECREF(current);
    }

    return 0;
}

static int
stacktrace_prepare(struct sr_py_report *report, PyTypeObject *type, bool multi_thread)
{
    if (!PyObject_TypeCheck(report->stacktrace, type))
    {
        PyErr_Format(PyExc_TypeError, "stacktrace must be an %s object", type->tp_name);
        return -1;
    }

    if (multi_thread)
    {
        report->report->stacktrace =
            ((struct sr_py_multi_stacktrace *)report->stacktrace)->stacktrace;
    }
    else
    {
        report->report->stacktrace =
            (struct sr_stacktrace *)((struct sr_py_base_thread *)report->stacktrace)->thread;
    }

    return 0;
}

static int
report_prepare_subobjects(struct sr_py_report *report)
{
    /* packages */
    if (rpms_prepare_linked_list(report) < 0)
        return -1;

    /* operating_system */
    if (report->operating_system == Py_None)
    {
        report->report->operating_system = NULL;
    }
    else
    {
        if (!PyObject_TypeCheck(report->operating_system, &sr_py_operating_system_type))
        {
            PyErr_SetString(PyExc_TypeError, "operating_system must be an OperatingSystem object");
            return -1;
        }

        report->report->operating_system =
            ((struct sr_py_operating_system *)report->operating_system)->operating_system;
    }

    /* stacktrace */
    if (report->stacktrace == Py_None)
    {
        report->report->stacktrace = NULL;
    }
    else
    {
        switch (report->report->report_type)
        {
        case SR_REPORT_CORE:
            if(stacktrace_prepare(report, &sr_py_core_stacktrace_type, true) < 0)
                return -1;
            break;
        case SR_REPORT_JAVA:
            if (stacktrace_prepare(report, &sr_py_java_stacktrace_type, true) < 0)
                return -1;
            break;
        case SR_REPORT_PYTHON:
            if (stacktrace_prepare(report, &sr_py_python_stacktrace_type, false) < 0)
                return -1;
            break;
        case SR_REPORT_KERNELOOPS:
            if (stacktrace_prepare(report, &sr_py_koops_stacktrace_type, false) < 0)
                return -1;
            break;
        default:
            report->report->stacktrace = NULL;
            break;
        }
    }

    return 0;
}

PyObject *
report_to_python_obj(struct sr_report *report)
{
    struct sr_py_report *ro =
        PyObject_New(struct sr_py_report, &sr_py_report_type);
    if (!ro)
        return PyErr_NoMemory();

    ro->report = report;

    /* wrap operating system in a python object */
    if (report->operating_system)
    {
        struct sr_py_operating_system *os =
            PyObject_New(struct sr_py_operating_system, &sr_py_operating_system_type);
        if (!os)
            return NULL;

        os->operating_system = report->operating_system;
        ro->operating_system = (PyObject *)os;
    }
    else
    {
        Py_INCREF(Py_None);
        ro->operating_system = Py_None;
    }

    /* packages */
    ro->packages = rpms_to_python_list(report->rpm_packages);
    if (ro->packages == NULL)
        return NULL;

    /* stacktrace */
    /* XXX: this could be rewritten in a generic way */
    if (report->stacktrace != NULL)
    {
        switch (report->report_type)
        {
        case SR_REPORT_CORE:
            ro->stacktrace = core_stacktrace_to_python_obj(
                    (struct sr_core_stacktrace *)report->stacktrace);
            break;
        case SR_REPORT_JAVA:
            ro->stacktrace = java_stacktrace_to_python_obj(
                    (struct sr_java_stacktrace *)report->stacktrace);
            break;
        case SR_REPORT_PYTHON:
            ro->stacktrace = python_stacktrace_to_python_obj(
                    (struct sr_python_stacktrace *)report->stacktrace);
            break;
        case SR_REPORT_KERNELOOPS:
            ro->stacktrace = koops_stacktrace_to_python_obj(
                    (struct sr_koops_stacktrace *)report->stacktrace);
            break;
        default:
            Py_INCREF(Py_None);
            ro->stacktrace = Py_None;
            break;
        }

        if (ro->stacktrace == NULL)
            return NULL;
    }
    else
    {
        Py_INCREF(Py_None);
        ro->stacktrace = Py_None;
    }

    return (PyObject *)ro;
}

/* constructor */
PyObject *
sr_py_report_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    const char *report_json = NULL;
    if (!PyArg_ParseTuple(args, "|s", &report_json))
        return NULL;

    struct sr_report *report;

    if (report_json)
    {
        char *error_msg;
        report = sr_report_from_json_text(report_json, &error_msg);
        if (!report)
        {
            PyErr_SetString(PyExc_ValueError, error_msg);
            free(error_msg);
            return NULL;
        }
    }
    else
    {
        report = sr_report_new();
        report->operating_system = sr_operating_system_new();
    }

    return report_to_python_obj(report);
}

/* destructor */
void
sr_py_report_free(PyObject *object)
{
    struct sr_py_report *this = (struct sr_py_report *)object;

    /* python will free the subordinate objects once their refcount reaches zero */
    /* XXX: is this correct wrt the warning on
     * http://docs.python.org/2.7/c-api/refcounting.html?highlight=py_decref#Py_DECREF ? */
    Py_DECREF(this->packages);
    Py_DECREF(this->operating_system);
    Py_DECREF(this->stacktrace);

    /* if there was no other reference, these structs are free'd by now */
    this->report->rpm_packages = NULL;
    this->report->operating_system = NULL;
    this->report->stacktrace = NULL;

    sr_report_free(this->report);
    PyObject_Del(object);
}

/* str */
PyObject *
sr_py_report_str(PyObject *object)
{
    struct sr_py_report *this = (struct sr_py_report*)object;
    struct sr_strbuf *buf = sr_strbuf_new();

    char *type = sr_report_type_to_string(this->report->report_type);
    sr_strbuf_append_strf(buf, "Report, type: %s", type);
    free(type);

    if (this->report->component_name)
        sr_strbuf_append_strf(buf, ", component: %s", this->report->component_name);

    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

PyObject *
sr_py_report_get_version(PyObject *self, void *data)
{
    return PyInt_FromLong(((struct sr_py_report*)self)->report->report_version);
}

PyObject *
sr_py_report_get_type(PyObject *self, void *data)
{
    struct sr_py_report *report = (struct sr_py_report *)self;

    char *type = sr_report_type_to_string(report->report->report_type);
    PyObject *res = PyString_FromString(type);

    free(type);
    return res;
}

int sr_py_report_set_type(PyObject *self, PyObject *rhs, void *data)
{
    struct sr_py_report *report = (struct sr_py_report *)self;

    if (rhs == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute.");
        return -1;
    }

    char *type_str = PyString_AsString(rhs);
    if (!type_str)
        return -1;

    enum sr_report_type type = sr_report_type_from_string(type_str);
    if (type == SR_REPORT_INVALID)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid report type.");
        return -1;
    }

    report->report->report_type = type;
    return 0;
}

PyObject *
sr_py_report_to_json(PyObject *self, PyObject *args)
{
    struct sr_py_report *this = (struct sr_py_report *)self;
    if (report_prepare_subobjects(this) < 0)
        return NULL;

    char *json = sr_report_to_json(this->report);
    if (!json)
        return NULL;

    PyObject *result = PyString_FromString(json);
    free(json);
    return result;
}
