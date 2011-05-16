#include <Python.h>

# inspiration: python-pycurl

struct backtrace_object {
    PyObject_HEAD
    PyObject *dict;                 /* Python attributes dictionary */
    CURL *handle;
    PyThreadState *state;
    CurlMultiObject *multi_stack;
    CurlShareObject *share;
    struct curl_httppost *httppost;
    struct curl_slist *httpheader;
    struct curl_slist *http200aliases;
    struct curl_slist *quote;
    struct curl_slist *postquote;
    struct curl_slist *prequote;
    /* callbacks */
    PyObject *w_cb;
    PyObject *h_cb;
    PyObject *r_cb;
    PyObject *pro_cb;
    PyObject *debug_cb;
    PyObject *ioctl_cb;
    PyObject *opensocket_cb;
    /* file objects */
    PyObject *readdata_fp;
    PyObject *writedata_fp;
    PyObject *writeheader_fp;
    /* misc */
    void *options[OPTIONS_SIZE];    /* for OBJECTPOINT options */
    char error[CURL_ERROR_SIZE+1];
};

static PyTypeObject backtrace_type = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "btparser.Backtrace",       /* tp_name */
    sizeof(struct backtrace_object),         /* tp_basicsize */
    0,                          /* tp_itemsize */
    /* Methods */
    (destructor)do_curl_dealloc,   /* tp_dealloc */
    0,                          /* tp_print */
    (getattrfunc)do_curl_getattr,  /* tp_getattr */
    (setattrfunc)do_curl_setattr,  /* tp_setattr */
    0,                          /* tp_compare */
    0,                          /* tp_repr */
    0,                          /* tp_as_number */
    0,                          /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash */
    0,                          /* tp_call */
    0,                          /* tp_str */
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
    0,                          /* tp_as_buffer */
    Py_TPFLAGS_HAVE_GC,         /* tp_flags */
    0,                          /* tp_doc */
    (traverseproc)do_curl_traverse, /* tp_traverse */
    (inquiry)do_curl_clear      /* tp_clear */
    /* More fields follow here, depending on your Python version. You can
     * safely ignore any compiler warnings about missing initializers.
     */
};

static char backtrace_new_doc [] =
    "Backtrace() -> New backtrace object.\n";

static PyObject *
backtrace_new(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
    sts = system(command);
    return Py_BuildValue("i", sts);
}

static char backtrace_parse_doc [] =
    "parse() -> tuple. Returns 2-tuple with new Backtrace object and"
    "position.\n";
static PyObject *
backtrace_parse(PyObject *self, PyObject *args)
{
}

static PyMethodDef btparser_methods[] = {
    {"Backtrace",  backtrace_new, METH_NOARGS, backtrace_new_doc},
    {"parse", backtrace_parse, METHOD_VARARGS, backtrace_parse_doc},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static char btparser_doc [] =
    "This module implements an interface to the btparser library.\n"
    "\n"
    "Types:\n"
    "\n"
    "Backtrace() -> New object.  Create a new empty backtrace.\n";

static PyObject *btparser_error;

PyMODINIT_FUNC
initbtparser(void)
{
    PyObject *m;

    m = Py_InitModule3("btparser", btparser_methods, btparser_doc);
    if (m == NULL)
        return;

    btparser_error = PyErr_NewException("btparser.error", NULL, NULL);
    Py_INCREF(btparser_error);
    PyModule_AddObject(m, "error", btparser_error);
}
