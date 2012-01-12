#include <Python.h>

#include "common.h"

static PyMethodDef module_methods[] = {
    { NULL },
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC init_btparser()
{
    if (PyType_Ready(&FrameTypeObject) < 0)
    {
        puts("PyType_Ready(&FrameTypeObject) < 0");
        return;
    }

    if (PyType_Ready(&ThreadTypeObject) < 0)
    {
        puts("PyType_Ready(&ThreadTypeObject) < 0");
        return;
    }

    if (PyType_Ready(&BacktraceTypeObject) < 0)
    {
        puts("PyType_Ready(&BacktraceTypeObject) < 0");
        return;
    }

    if (PyType_Ready(&DistancesTypeObject) < 0)
    {
        puts("PyType_Ready(&DistancesTypeObject) < 0");
        return;
    }

    if (PyType_Ready(&DendrogramTypeObject) < 0)
    {
        puts("PyType_Ready(&DendrogramTypeObject) < 0");
        return;
    }

    if (PyType_Ready(&SharedlibTypeObject) < 0)
    {
        puts("PyType_Ready(&SharedlibTypeObject) < 0");
        return;
    }

    PyObject *module = Py_InitModule("_btparser", module_methods);
    if (!module)
    {
        puts("module == NULL");
        return;
    }

    Py_INCREF(&FrameTypeObject);
    PyModule_AddObject(module, "Frame", (PyObject *)&FrameTypeObject);
    Py_INCREF(&ThreadTypeObject);
    PyModule_AddObject(module, "Thread", (PyObject *)&ThreadTypeObject);
    Py_INCREF(&BacktraceTypeObject);
    PyModule_AddObject(module, "Backtrace", (PyObject *)&BacktraceTypeObject);
    Py_INCREF(&DistancesTypeObject);
    PyModule_AddObject(module, "Distances", (PyObject *)&DistancesTypeObject);
    Py_INCREF(&DendrogramTypeObject);
    PyModule_AddObject(module, "Dendrogram", (PyObject *)&DendrogramTypeObject);
    Py_INCREF(&SharedlibTypeObject);
    PyModule_AddObject(module, "Sharedlib", (PyObject *)&SharedlibTypeObject);
}
