#include <Python.h>
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
#include "py_metrics.h"

#include "distance.h"
#include "gdb/sharedlib.h"

static PyMethodDef
module_methods[]=
{
    { NULL },
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC
init_satyr()
{
    if (PyType_Ready(&sr_py_base_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_base_frame_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_base_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_base_thread_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_single_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_single_stacktrace_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_multi_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_multi_stacktrace_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_gdb_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_frame_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_gdb_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_thread_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_gdb_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_stacktrace_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_distances_type) < 0)
    {
        puts("PyType_Ready(&sr_py_distances_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_dendrogram_type) < 0)
    {
        puts("PyType_Ready(&sr_py_dendrogram_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_gdb_sharedlib_type) < 0)
    {
        puts("PyType_Ready(&sr_py_gdb_sharedlib_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_koops_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_koops_frame_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_koops_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_koops_stacktrace_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_python_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_python_frame_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_python_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_python_stacktrace_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_java_frame_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_frame_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_java_thread_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_thread_type) < 0");
        return;
    }

    if (PyType_Ready(&sr_py_java_stacktrace_type) < 0)
    {
        puts("PyType_Ready(&sr_py_java_stacktrace_type) < 0");
        return;
    }


    PyObject *module = Py_InitModule("_satyr", module_methods);
    if (!module)
    {
        puts("module == NULL");
        return;
    }

    Py_INCREF(&sr_py_base_frame_type);
    PyModule_AddObject(module, "BaseFrame",
                       (PyObject *)&sr_py_base_frame_type);

    Py_INCREF(&sr_py_base_thread_type);
    PyModule_AddObject(module, "BaseThread",
                       (PyObject *)&sr_py_base_thread_type);

    Py_INCREF(&sr_py_single_stacktrace_type);
    PyModule_AddObject(module, "SingleThreadStacktrace",
                       (PyObject *)&sr_py_single_stacktrace_type);

    Py_INCREF(&sr_py_multi_stacktrace_type);
    PyModule_AddObject(module, "MultiThreadStacktrace",
                       (PyObject *)&sr_py_multi_stacktrace_type);

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

}
