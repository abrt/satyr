#include <Python.h>
#include "py_common.h"
#include "py_cluster.h"
#include "py_base_frame.h"
#include "py_base_thread.h"
#include "py_base_stacktrace.h"
#include "py_gdb_frame.h"
#include "py_gdb_sharedlib.h"
#include "py_gdb_stacktrace.h"
#include "py_gdb_thread.h"
#include "py_koops_frame.h"
#include "py_koops_stacktrace.h"
#include "py_python_frame.h"
#include "py_python_stacktrace.h"
#include "py_java_frame.h"
#include "py_java_thread.h"
#include "py_java_stacktrace.h"
#include "py_core_frame.h"
#include "py_core_thread.h"
#include "py_core_stacktrace.h"
#include "py_ruby_frame.h"
#include "py_ruby_stacktrace.h"
#include "py_js_frame.h"
#include "py_js_stacktrace.h"
#include "py_rpm_package.h"
#include "py_metrics.h"
#include "py_operating_system.h"
#include "py_report.h"

#include "distance.h"
#include "thread.h"
#include "stacktrace.h"
#include "gdb/sharedlib.h"
#include "rpm.h"
#include "utils.h"

#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT PyMODINIT_FUNC PyInit__satyr3(void)
  #define MOD_DEF(ob, name, doc, methods) \
            static struct PyModuleDef moduledef = { \
              PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
            ob = PyModule_Create(&moduledef);
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT void init_satyr(void)
  #define MOD_DEF(ob, name, doc, methods) \
            ob = Py_InitModule3(name, methods, doc);
#endif

/* XXX: if we export more standalone functions, move them to a separate file */
static PyObject *
sr_py_demangle_symbol(PyObject *self, PyObject *args)
{
    char *mangled;
    PyObject *result;

    if (!PyArg_ParseTuple(args, "s", &mangled))
        return NULL;

    char *demangled = sr_demangle_symbol(mangled);

    if (demangled)
    {
        result = PyString_FromString(demangled);
        free(demangled);
    }
    else
        result = PyString_FromString(mangled);

    return result;
}


static PyMethodDef
module_methods[]=
{
    { "demangle_symbol", sr_py_demangle_symbol, METH_VARARGS, "Demangle C++ symbol." },
    { NULL },
};

static char module_doc[] = "Anonymous, machine-friendly problem reports library";

MOD_INIT
{
    if (PyType_Ready(&sr_py_base_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_base_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_base_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_base_thread_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_single_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_single_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_multi_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_multi_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_gdb_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_gdb_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_thread_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_gdb_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_distances_type) < 0)
    {
        puts("PyType_Ready(&sr_py_distances_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_distances_part_type) < 0)
    {
        puts("PyType_Ready(&sr_py_distances_part_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_dendrogram_type) < 0)
    {
        puts("PyType_Ready(&sr_py_dendrogram_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_gdb_sharedlib_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_sharedlib_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_koops_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_koops_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_koops_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_koops_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_python_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_python_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_python_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_python_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_java_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_java_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_thread_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_java_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_core_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_core_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_core_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_core_thread_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_core_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_core_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_ruby_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_ruby_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_ruby_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_ruby_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_js_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_js_frame_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_js_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_js_stacktrace_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_operating_system_type) < 0)
    {
        puts("PyType_Ready(&sr_py_operating_system_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_report_type) < 0)
    {
        puts("PyType_Ready(&sr_py_report_type) < 0");
        return MOD_ERROR_VAL;
    }

    if (PyType_Ready(&sr_py_rpm_package_type) < 0)
    {
        puts("PyType_Ready(&sr_py_rpm_package_type) < 0");
        return MOD_ERROR_VAL;
    }


    PyObject *module;
    MOD_DEF(module, "_satyr", module_doc, module_methods);
    if (!module)
    {
        puts("module == NULL");
        return MOD_ERROR_VAL;
    }

    Py_INCREF(&sr_py_base_frame_type);
    PyModule_AddObject(module, "BaseFrame",
                       (PyObject *)&sr_py_base_frame_type);

    Py_INCREF(&sr_py_base_thread_type);
    PyModule_AddObject(module, "BaseThread",
                       (PyObject *)&sr_py_base_thread_type);

    PyModule_AddIntConstant(module, "DUPHASH_NORMAL", SR_DUPHASH_NORMAL);
    PyModule_AddIntConstant(module, "DUPHASH_NOHASH", SR_DUPHASH_NOHASH);
    PyModule_AddIntConstant(module, "DUPHASH_NONORMALIZE", SR_DUPHASH_NONORMALIZE);
    PyModule_AddIntConstant(module, "DUPHASH_KOOPS_COMPAT", SR_DUPHASH_KOOPS_COMPAT);

    Py_INCREF(&sr_py_single_stacktrace_type);
    PyModule_AddObject(module, "SingleThreadStacktrace",
                       (PyObject *)&sr_py_single_stacktrace_type);

    Py_INCREF(&sr_py_multi_stacktrace_type);
    PyModule_AddObject(module, "MultiThreadStacktrace",
                       (PyObject *)&sr_py_multi_stacktrace_type);

    PyModule_AddIntConstant(module, "BTHASH_NORMAL", SR_BTHASH_NORMAL);
    PyModule_AddIntConstant(module, "BTHASH_NOHASH", SR_BTHASH_NOHASH);

    Py_INCREF(&sr_py_gdb_frame_type);
    PyModule_AddObject(module, "GdbFrame",
                       (PyObject *)&sr_py_gdb_frame_type);

    Py_INCREF(&sr_py_gdb_thread_type);
    PyModule_AddObject(module, "GdbThread",
                       (PyObject *)&sr_py_gdb_thread_type);

    Py_INCREF(&sr_py_gdb_stacktrace_type);
    PyModule_AddObject(module, "GdbStacktrace",
                       (PyObject *)&sr_py_gdb_stacktrace_type);

    Py_INCREF(&sr_py_distances_type);
    PyModule_AddObject(module, "Distances",
                       (PyObject *)&sr_py_distances_type);

    Py_INCREF(&sr_py_distances_part_type);
    PyModule_AddObject(module, "DistancesPart",
                       (PyObject *)&sr_py_distances_part_type);

    PyModule_AddIntConstant(module, "DISTANCE_JARO_WINKLER",
                            SR_DISTANCE_JARO_WINKLER);
    PyModule_AddIntConstant(module, "DISTANCE_JACCARD", SR_DISTANCE_JACCARD);
    PyModule_AddIntConstant(module, "DISTANCE_LEVENSHTEIN",
                            SR_DISTANCE_LEVENSHTEIN);
    PyModule_AddIntConstant(module, "DISTANCE_DAMERAU_LEVENSHTEIN",
                            SR_DISTANCE_DAMERAU_LEVENSHTEIN);

    Py_INCREF(&sr_py_dendrogram_type);
    PyModule_AddObject(module, "Dendrogram",
                       (PyObject *)&sr_py_dendrogram_type);

    Py_INCREF(&sr_py_gdb_sharedlib_type);
    PyModule_AddObject(module, "GdbSharedlib",
                       (PyObject *)&sr_py_gdb_sharedlib_type);

    PyModule_AddIntMacro(module, SYMS_OK);
    PyModule_AddIntMacro(module, SYMS_NOT_FOUND);
    PyModule_AddIntMacro(module, SYMS_WRONG);

    Py_INCREF(&sr_py_koops_frame_type);
    PyModule_AddObject(module, "KerneloopsFrame",
                       (PyObject *)&sr_py_koops_frame_type);

    Py_INCREF(&sr_py_koops_stacktrace_type);
    PyModule_AddObject(module, "Kerneloops",
                       (PyObject *)&sr_py_koops_stacktrace_type);

    Py_INCREF(&sr_py_python_frame_type);
    PyModule_AddObject(module, "PythonFrame",
                       (PyObject *)&sr_py_python_frame_type);

    Py_INCREF(&sr_py_python_stacktrace_type);
    PyModule_AddObject(module, "PythonStacktrace",
                       (PyObject *)&sr_py_python_stacktrace_type);

    Py_INCREF(&sr_py_java_frame_type);
    PyModule_AddObject(module, "JavaFrame",
                       (PyObject *)&sr_py_java_frame_type);

    Py_INCREF(&sr_py_java_thread_type);
    PyModule_AddObject(module, "JavaThread",
                       (PyObject *)&sr_py_java_thread_type);

    Py_INCREF(&sr_py_java_stacktrace_type);
    PyModule_AddObject(module, "JavaStacktrace",
                       (PyObject *)&sr_py_java_stacktrace_type);

    Py_INCREF(&sr_py_ruby_frame_type);
    PyModule_AddObject(module, "RubyFrame",
                       (PyObject *)&sr_py_ruby_frame_type);

    Py_INCREF(&sr_py_ruby_stacktrace_type);
    PyModule_AddObject(module, "RubyStacktrace",
                       (PyObject *)&sr_py_ruby_stacktrace_type);

    Py_INCREF(&sr_py_js_frame_type);
    PyModule_AddObject(module, "JavaScriptFrame",
                       (PyObject *)&sr_py_js_frame_type);

    Py_INCREF(&sr_py_js_stacktrace_type);
    PyModule_AddObject(module, "JavaScriptStacktrace",
                       (PyObject *)&sr_py_js_stacktrace_type);

    Py_INCREF(&sr_py_core_frame_type);
    PyModule_AddObject(module, "CoreFrame",
                       (PyObject *)&sr_py_core_frame_type);

    Py_INCREF(&sr_py_core_thread_type);
    PyModule_AddObject(module, "CoreThread",
                       (PyObject *)&sr_py_core_thread_type);

    Py_INCREF(&sr_py_core_stacktrace_type);
    PyModule_AddObject(module, "CoreStacktrace",
                       (PyObject *)&sr_py_core_stacktrace_type);

    Py_INCREF(&sr_py_operating_system_type);
    PyModule_AddObject(module, "OperatingSystem",
                       (PyObject *)&sr_py_operating_system_type);

    Py_INCREF(&sr_py_report_type);
    PyModule_AddObject(module, "Report", (PyObject *)&sr_py_report_type);

    Py_INCREF(&sr_py_rpm_package_type);
    PyModule_AddObject(module, "RpmPackage",
                       (PyObject *)&sr_py_rpm_package_type);

    PyModule_AddIntConstant(module, "ROLE_UNKNOWN", SR_ROLE_UNKNOWN);
    PyModule_AddIntConstant(module, "ROLE_AFFECTED", SR_ROLE_AFFECTED);

    return MOD_SUCCESS_VAL(module);
}
