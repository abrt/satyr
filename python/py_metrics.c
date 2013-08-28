#include "py_metrics.h"
#include "py_base_thread.h"
#include "strbuf.h"
#include "distance.h"

#define distances_doc "satyr.Distances - class representing distances between objects\n\n" \
                      "Usage:\n\n" \
                      "satyr.Distances(m, n) - creates an m-by-n distance matrix\n\n" \
                      "satyr.Distances([threads], m, dist_type=DISTANCE_LEVENSHTEIN) "\
                      "- compares first m threads with others\n\n" \
                      "dist_type (optional): DISTANCE_LEVENSHTEIN, DISTANCE_JACCARD "\
                      "or DISTANCE_DAMERAU_LEVENSHTEIN"

#define di_get_size_doc "Usage: distances.get_size()\n\n" \
                        "Returns: (m, n) - size of the distance matrix"

#define di_get_distance_doc "Usage: distances.get_distance(i, j)\n\n" \
                            "Returns: positive float - distance between objects i and j"

#define di_set_distance_doc "Usage: distances.set_distance(i, j, d)\n\n" \
                            "Sets distance between objects i and j to d"

#define di_dup_doc "Usage: distances.dup()\n\n" \
                   "Returns: satyr.Distances - a new clone of the distances\n\n" \
                   "Clones the distances object. All new structures are independent\n" \
                   "of the original object."

static PyMethodDef
distances_methods[] =
{
    /* getters & setters */
    { "get_size",       sr_py_distances_get_size,     METH_NOARGS,  di_get_size_doc      },
    { "get_distance",   sr_py_distances_get_distance, METH_VARARGS, di_get_distance_doc  },
    { "set_distance",   sr_py_distances_set_distance, METH_VARARGS, di_set_distance_doc  },
    /* methods */
    { "dup",            sr_py_distances_dup,          METH_NOARGS,  di_dup_doc           },
    { NULL },
};

PyTypeObject
sr_py_distances_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.Distances",       /* tp_name */
    sizeof(struct sr_py_distances),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_distances_free,       /* tp_dealloc */
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
    sr_py_distances_str,        /* tp_str */
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
    distances_methods,          /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_distances_new,        /* tp_new */
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
sr_py_distances_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_distances *o = (struct sr_py_distances*)
        PyObject_New(struct sr_py_distances, &sr_py_distances_type);

    if (!o)
        return PyErr_NoMemory();

    PyObject * thread_list;
    int i, m, n;
    int dist_type = SR_DISTANCE_LEVENSHTEIN;
    static const char *kwlist[] = { "threads", "m", "dist_type", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O!i|i", (char **)kwlist,
                                    &PyList_Type, &thread_list, &m, &dist_type))
    {
        n = PyList_Size(thread_list);
        struct sr_thread *threads[n];
        PyTypeObject *thread_type = NULL;

        for (i = 0; i < n; i++)
        {
            PyObject *obj = PyList_GetItem(thread_list, i);
            if (!PyObject_TypeCheck(obj, &sr_py_base_thread_type))
            {
                PyErr_SetString(PyExc_TypeError, "Must be a list of satyr.BaseThread objects");
                return NULL;
            }

            /* check that the type is the same as in the previous thread */
            if (thread_type && obj->ob_type != thread_type)
            {
                PyErr_SetString(PyExc_TypeError, "All threads in the list must have the same type");
                return NULL;
            }
            thread_type = obj->ob_type;

            struct sr_py_base_thread *to = (struct sr_py_base_thread*)obj;
            if (frames_prepare_linked_list(to) < 0)
            {
                return NULL;
            }
            threads[i] = (struct sr_thread*)to->thread;
        }
        if (m < 1 || n < 2)
        {
            PyErr_SetString(PyExc_ValueError, "Distance matrix must have at least 1 row and 2 columns");
            return NULL;
        }

        if (dist_type < 0 || dist_type >= SR_DISTANCE_NUM)
        {
            PyErr_SetString(PyExc_ValueError, "Invalid distance type");
            return NULL;
        }

        if (dist_type == SR_DISTANCE_JARO_WINKLER)
        {
            PyErr_SetString(PyExc_ValueError, "Cannot use DISTANCE_JARO_WINKLER as it is not a metric");
            return NULL;
        }

        o->distances = sr_threads_compare(threads, m, n, dist_type);
    }
    else if (PyArg_ParseTuple(args, "ii", &m, &n))
    {
        PyErr_Clear();
        if (m < 1 || n < 2)
        {
            PyErr_SetString(PyExc_ValueError, "Distance matrix must have at least 1 row and 2 columns");
            return NULL;
        }

        o->distances = sr_distances_new(m, n);
    }
    else
        return NULL;

    return (PyObject *)o;
}

/* destructor */
void
sr_py_distances_free(PyObject *object)
{
    struct sr_py_distances *this = (struct sr_py_distances *)object;
    sr_distances_free(this->distances);
    PyObject_Del(object);
}

PyObject *
sr_py_distances_str(PyObject *self)
{
    struct sr_py_distances *this = (struct sr_py_distances *)self;
    struct sr_strbuf *buf = sr_strbuf_new();
    sr_strbuf_append_strf(buf, "%d-by-%d distance matrix",
                           this->distances->m, this->distances->n);
    char *str = sr_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */
PyObject *
sr_py_distances_get_size(PyObject *self, PyObject *args)
{
    struct sr_py_distances *this = (struct sr_py_distances *)self;
    return Py_BuildValue("ii", this->distances->m, this->distances->n);
}

PyObject *
sr_py_distances_get_distance(PyObject *self, PyObject *args)
{
    struct sr_py_distances *this = (struct sr_py_distances *)self;
    int i, j;
    if (!PyArg_ParseTuple(args, "ii", &i, &j))
        return NULL;

    if (i < 0 || j < 0 || i >= this->distances->m || j >= this->distances->n)
    {
        PyErr_SetString(PyExc_ValueError, "Distance matrix index out of range");
        return NULL;
    }

    return Py_BuildValue("f", sr_distances_get_distance(this->distances, i, j));
}

PyObject *
sr_py_distances_set_distance(PyObject *self, PyObject *args)
{
    struct sr_py_distances *this = (struct sr_py_distances *)self;
    int i, j;
    float f;
    if (!PyArg_ParseTuple(args, "iif", &i, &j, &f))
        return NULL;

    if (i < 0 || j < 0 || i >= this->distances->m || j >= this->distances->n)
    {
        PyErr_SetString(PyExc_ValueError, "Distance matrix index out of range");
        return NULL;
    }

    sr_distances_set_distance(this->distances, i, j, f);
    Py_RETURN_NONE;
}

/* methods */
PyObject *
sr_py_distances_dup(PyObject *self, PyObject *args)
{
    struct sr_py_distances *this = (struct sr_py_distances*)self;
    struct sr_py_distances *o = (struct sr_py_distances*)
        PyObject_New(struct sr_py_distances, &sr_py_distances_type);

    if (!o)
        return PyErr_NoMemory();

    o->distances = sr_distances_dup(this->distances);
    if (!o->distances)
        return NULL;

    return (PyObject*)o;
}
