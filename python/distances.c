#include "common.h"
#include "strbuf.h"

static PyMethodDef DistancesMethods[] = {
    /* getters & setters */
    { "get_size",       p_btp_distances_get_size,     METH_NOARGS,  di_get_size_doc      },
    { "get_distance",   p_btp_distances_get_distance, METH_VARARGS, di_get_distance_doc  },
    { "set_distance",   p_btp_distances_set_distance, METH_VARARGS, di_set_distance_doc  },
    /* methods */
    { "dup",            p_btp_distances_dup,          METH_NOARGS,  di_dup_doc           },
    { NULL },
};

PyTypeObject DistancesTypeObject = {
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Distances",       /* tp_name */
    sizeof(DistancesObject),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    p_btp_distances_free,       /* tp_dealloc */
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
    p_btp_distances_str,        /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    distances_doc,              /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    DistancesMethods,           /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    p_btp_distances_new,        /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* constructor */
PyObject *p_btp_distances_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    DistancesObject *o = (DistancesObject *)PyObject_New(DistancesObject, &DistancesTypeObject);
    if (!o)
        return PyErr_NoMemory();

    PyObject * thread_list;
    int i, m, n;
    const char *dist_name;

    if (PyArg_ParseTuple(args, "sO!i", &dist_name, &PyList_Type, &thread_list, &m))
    {
        n = PyList_Size(thread_list);
        struct btp_thread *threads[n];

        for (i = 0; i < n; i++)
        {
            PyObject *obj = PyList_GetItem(thread_list, i);
            if (!PyObject_TypeCheck(obj, &ThreadTypeObject))
            {
                PyErr_SetString(PyExc_TypeError, "Must be a list of btparser.Thread objects");
                return NULL;
            }
            threads[i] = ((ThreadObject *)obj)->thread;
        }
        if (m < 1 || n < 2)
        {
            PyErr_SetString(PyExc_ValueError, "Distance matrix must have at least 1 row and 2 columns");
            return NULL;
        }

        btp_dist_thread_type dist_func;

        if (!strcmp(dist_name, "jaccard"))
            dist_func = btp_thread_jaccard_distance;
        else if (!strcmp(dist_name, "levenshtein"))
            dist_func = btp_thread_levenshtein_distance_f;
        else
        {
            PyErr_SetString(PyExc_ValueError, "Unknown name of distance function");
            return NULL;
        }

        o->distances = btp_threads_compare(threads, m, n, dist_func);
    }
    else if (PyArg_ParseTuple(args, "ii", &m, &n))
    {
        PyErr_Clear();
        if (m < 1 || n < 2)
        {
            PyErr_SetString(PyExc_ValueError, "Distance matrix must have at least 1 row and 2 columns");
            return NULL;
        }

        o->distances = btp_distances_new(m, n);
    }
    else
        return NULL;

    return (PyObject *)o;
}

/* destructor */
void p_btp_distances_free(PyObject *object)
{
    DistancesObject *this = (DistancesObject *)object;
    btp_distances_free(this->distances);
    PyObject_Del(object);
}

PyObject *p_btp_distances_str(PyObject *self)
{
    DistancesObject *this = (DistancesObject *)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "%d-by-%d distance matrix",
                           this->distances->m, this->distances->n);
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */
PyObject *p_btp_distances_get_size(PyObject *self, PyObject *args)
{
    DistancesObject *this = (DistancesObject *)self;
    return Py_BuildValue("ii", this->distances->m, this->distances->n);
}

PyObject *p_btp_distances_get_distance(PyObject *self, PyObject *args)
{
    DistancesObject *this = (DistancesObject *)self;
    int i, j;
    if (!PyArg_ParseTuple(args, "ii", &i, &j))
        return NULL;

    if (i < 0 || j < 0 || i >= this->distances->m || j >= this->distances->n)
    {
        PyErr_SetString(PyExc_ValueError, "Distance matrix index out of range");
        return NULL;
    }

    return Py_BuildValue("f", btp_distances_get_distance(this->distances, i, j));
}

PyObject *p_btp_distances_set_distance(PyObject *self, PyObject *args)
{
    DistancesObject *this = (DistancesObject *)self;
    int i, j;
    float f;
    if (!PyArg_ParseTuple(args, "iif", &i, &j, &f))
        return NULL;

    if (i < 0 || j < 0 || i >= this->distances->m || j >= this->distances->n)
    {
        PyErr_SetString(PyExc_ValueError, "Distance matrix index out of range");
        return NULL;
    }

    btp_distances_set_distance(this->distances, i, j, f);
    Py_RETURN_NONE;
}

/* methods */
PyObject *p_btp_distances_dup(PyObject *self, PyObject *args)
{
    DistancesObject *this = (DistancesObject *)self;
    DistancesObject *o = (DistancesObject *)PyObject_New(DistancesObject, &DistancesTypeObject);
    if (!o)
        return PyErr_NoMemory();

    o->distances = btp_distances_dup(this->distances);
    if (!o->distances)
        return NULL;

    return (PyObject *)o;
}
