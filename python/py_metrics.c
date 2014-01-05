#include "py_metrics.h"
#include "py_base_thread.h"
#include "py_common.h"
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

#define di_merge_parts_doc "Usage: satyr.Distances.merge_parts([distance_parts])\n\n" \
                           "Returns: new satyr.Distances object constructed from distance matrix parts " \
                           "that were created by satyr.DistancesPart.create and their compute() method " \
                           "has been called."

#define distances_part_doc "satyr.DistancesPart - class representing a part of a distance matrix " \
                           "that can be computed independent of other parts and later merged into " \
                           "the full matrix.\n\n" \
                           "This class does not have a constructor, use satyr.DistancePart.create " \
                           "to create a list of distance matrix parts."

#define dip_create "Usage: satyr.Distances.create(n, nparts, m=(n-1), dist_type=DISTANCE_LEVENSHTEIN)\n\n" \
                   "Returns: list of at most nparts satyr.DistancesPart objects for m-by-n distance " \
                   "matrix that is to be computed using dist_type metric." \

#define dip_compute "Usage: distancespart.compute([threads])\n\n" \
                    "Returns: None\n\n" \
                    "Computes the part of the distance matrix. Make sure to pass the threads " \
                    "list in the same order to every part."

#define dip_reduce "Used for pickling."

static PyMethodDef
distances_methods[] =
{
    /* getters & setters */
    { "get_size",       sr_py_distances_get_size,     METH_NOARGS,              di_get_size_doc     },
    { "get_distance",   sr_py_distances_get_distance, METH_VARARGS,             di_get_distance_doc },
    { "set_distance",   sr_py_distances_set_distance, METH_VARARGS,             di_set_distance_doc },
    /* methods */
    { "dup",            sr_py_distances_dup,          METH_NOARGS,              di_dup_doc          },
    { "merge_parts",    sr_py_distances_merge_parts,  METH_VARARGS|METH_STATIC, di_merge_parts_doc  },
    { NULL },
};

static PyMethodDef
distances_part_methods[] =
{
    { "create",     (PyCFunction)sr_py_distances_part_create,  METH_VARARGS|METH_KEYWORDS|METH_STATIC, dip_create  },
    { "compute",    sr_py_distances_part_compute,              METH_VARARGS,                           dip_compute },
    { "__reduce__", sr_py_distances_part_reduce,               METH_VARARGS,                           dip_reduce  },
    { NULL },
};

PyTypeObject
sr_py_distances_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
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

PyTypeObject
sr_py_distances_part_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.DistancesPart",      /* tp_name */
    sizeof(struct sr_py_distances_part), /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_distances_part_free,  /* tp_dealloc */
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
    sr_py_distances_part_str,   /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    distances_part_doc,         /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    distances_part_methods,     /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    sr_py_distances_part_new,   /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

static bool
validate_distance_params(int m, int n, int dist_type)
{
    if (m < 1 || n < 2)
        PyErr_SetString(PyExc_ValueError, "Distance matrix must have at least 1 row and 2 columns");
    else if (dist_type < 0 || dist_type >= SR_DISTANCE_NUM)
        PyErr_SetString(PyExc_ValueError, "Invalid distance type");
    else if (dist_type == SR_DISTANCE_JARO_WINKLER)
        PyErr_SetString(PyExc_ValueError, "Cannot use DISTANCE_JARO_WINKLER as it is not a metric");
    else
        return true;

    return false;
}

static bool
prepare_thread_array(PyObject *thread_list, struct sr_thread *threads[], int n)
{
    int i;
    PyTypeObject *thread_type = NULL;

    for (i = 0; i < n; i++)
    {
        PyObject *obj = PyList_GetItem(thread_list, i);
        if (!PyObject_TypeCheck(obj, &sr_py_base_thread_type))
        {
            PyErr_SetString(PyExc_TypeError, "Must be a list of satyr.BaseThread objects");
            return false;
        }

        /* check that the type is the same as in the previous thread */
        if (thread_type && obj->ob_type != thread_type)
        {
            PyErr_SetString(PyExc_TypeError, "All threads in the list must have the same type");
            return false;
        }
        thread_type = obj->ob_type;

        struct sr_py_base_thread *to = (struct sr_py_base_thread*)obj;
        if (frames_prepare_linked_list(to) < 0)
            return false;

        threads[i] = (struct sr_thread*)to->thread;
    }

    return true;
}

/* constructor */
PyObject *
sr_py_distances_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    struct sr_py_distances *o = (struct sr_py_distances*)
        PyObject_New(struct sr_py_distances, &sr_py_distances_type);

    if (!o)
        return PyErr_NoMemory();

    PyObject *thread_list;
    int m, n;
    int dist_type = SR_DISTANCE_LEVENSHTEIN;
    static const char *kwlist[] = { "threads", "m", "dist_type", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O!i|i", (char **)kwlist,
                                    &PyList_Type, &thread_list, &m, &dist_type))
    {
        n = PyList_Size(thread_list);
        struct sr_thread *threads[n];

        if (!validate_distance_params(m, n, dist_type))
            return NULL;

        if (!prepare_thread_array(thread_list, threads, n))
            return NULL;

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

static struct sr_distances_part*
parts_prepare_linked_list(PyObject *part_list)
{
    int i;
    PyObject *item;
    struct sr_py_distances_part *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(part_list); ++i)
    {
        item = PyList_GetItem(part_list, i);
        if (!item)
            return NULL;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, &sr_py_distances_part_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_SetString(PyExc_TypeError,
                    "argument must be a list of satyr.DistancePart objects");
            return NULL;
        }

        current = (struct sr_py_distances_part*)item;
        if (i != 0)
            prev->distances_part->next = current->distances_part;

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        current->distances_part->next = NULL;
        Py_DECREF(current);
    }

    if (PyList_Size(part_list) > 0)
        return ((struct sr_py_distances_part*)PyList_GetItem(part_list, 0))->distances_part;
    else
        return NULL;
}

PyObject *
sr_py_distances_merge_parts(PyObject *self, PyObject *args)
{
    PyObject *part_list;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &part_list))
        return NULL;

    struct sr_distances_part *parts = parts_prepare_linked_list(part_list);
    if (!parts)
        return NULL;

    struct sr_distances *dist = sr_distances_part_merge(parts);
    if (!dist)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to merge distance matrix parts");
        return NULL;
    }

    struct sr_py_distances *o = PyObject_New(struct sr_py_distances, &sr_py_distances_type);
    if (!o)
        return PyErr_NoMemory();

    o->distances = dist;
    return (PyObject *)o;
}

/* constructor */
PyObject *
sr_py_distances_part_new(PyTypeObject *object, PyObject *args, PyObject *kwds)
{
    int m, n, m_begin, n_begin;
    unsigned long long len;
    enum sr_distance_type dist_type;
    unsigned long long checksum;
    PyObject *dist_list;

    if (!PyArg_ParseTuple(args, "iiiiKiKO", &m, &n, &m_begin, &n_begin, &len, &dist_type,
                          &checksum, &dist_list))
        return NULL;

    struct sr_distances_part *part = sr_distances_part_new(m, n, dist_type, m_begin, n_begin,
                                                           (size_t)len);
    part->checksum = (uint32_t)checksum;

    if (PyList_Check(dist_list))
    {
        part->distances = sr_malloc_array(sizeof(float), part->len);
        int i;
        for (i = 0; i < PyList_Size(dist_list); i++)
        {
            PyObject *item = PyList_GetItem(dist_list, i);
            if (!item)
                goto error;

            double d = PyFloat_AsDouble(item);
            if (PyErr_Occurred())
                goto error;

            part->distances[i] = (float)d;
        }
    }
    else if (dist_list != Py_None)
    {
        PyErr_SetString(PyExc_TypeError, "distances must be list of floats or None");
        goto error;
    }

    struct sr_py_distances_part *py_part =
        PyObject_New(struct sr_py_distances_part, &sr_py_distances_part_type);
    py_part->distances_part = part;

    return (PyObject *)py_part;
error:
    sr_distances_part_free(part, false);
    return NULL;
}

/* destructor */
void
sr_py_distances_part_free(PyObject *object)
{
    struct sr_py_distances_part *this = (struct sr_py_distances_part *)object;
    sr_distances_part_free(this->distances_part, false);
    PyObject_Del(object);
}

PyObject *
sr_py_distances_part_str(PyObject *self)
{
    struct sr_py_distances_part *this = (struct sr_py_distances_part *)self;
    struct sr_distances_part *part = this->distances_part;
    return PyString_FromFormat(
        "%d-by-%d distance matrix part starting at (%d, %d) of length %zu, %scomputed",
        part->m, part->n, part->m_begin, part->n_begin, part->len,
        (part->distances ? "" : "not "));
}

/* methods */
PyObject *
sr_py_distances_part_reduce(PyObject *self, PyObject *args)
{
    struct sr_py_distances_part *this = (struct sr_py_distances_part *)self;
    struct sr_distances_part *part = this->distances_part;

    PyObject *dist_list;
    if (part->distances)
    {
        dist_list = PyList_New(0);
        if (!dist_list)
            return NULL;

        int i;
        for (i = 0; i < part->len; i++)
        {
            PyObject *f = PyFloat_FromDouble((double)part->distances[i]);
            if (!f)
            {
                Py_XDECREF(dist_list);
                return NULL;
            }

            if (PyList_Append(dist_list, f) != 0)
            {
                Py_XDECREF(f);
                Py_XDECREF(dist_list);
                return NULL;
            }
        }
    }
    else
    {
        Py_INCREF(Py_None);
        dist_list = Py_None;
    }

    return Py_BuildValue("O(iiiiKiKN)", &sr_py_distances_part_type, part->m, part->n,
                         part->m_begin, part->n_begin, (unsigned long long)part->len,
                         part->dist_type, (unsigned long long)part->checksum, dist_list);
}

PyObject *
sr_py_distances_part_create(PyObject *self, PyObject *args, PyObject *kwds)
{
    int n, m = 0;
    int dist_type = SR_DISTANCE_LEVENSHTEIN;
    unsigned nparts;
    static const char *kwlist[] = { "n", "nparts", "m", "dist_type", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "iI|ii", (char**)kwlist,
                                     &n, &nparts, &m, &dist_type))
        return NULL;

    if (m == 0)
        m = n - 1;

    if (!validate_distance_params(m, n, dist_type))
        return NULL;

    struct sr_distances_part *parts = sr_distances_part_create(m, n, dist_type, nparts);
    if (parts == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create distance matrix parts");
        return NULL;
    }

    PyObject *part_list = PyList_New(0);
    struct sr_distances_part *it;
    for (it = parts; it != NULL; it = it->next)
    {
        struct sr_py_distances_part *py_part =
            PyObject_New(struct sr_py_distances_part, &sr_py_distances_part_type);

        py_part->distances_part = it;
        if (PyList_Append(part_list, (PyObject*)py_part) != 0)
        {
            /* Decrementing list refcount should free all its elements. */
            Py_XDECREF(part_list);
            /* This ought to free unprocessed elements. */
            sr_distances_part_free(it, true);
            return NULL;
        }
    }

    return part_list;
}

PyObject *
sr_py_distances_part_compute(PyObject *self, PyObject *args)
{
    struct sr_py_distances_part *this = (struct sr_py_distances_part*)self;
    PyObject *thread_list;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &thread_list))
        return NULL;

    int n = PyList_Size(thread_list);
    struct sr_thread *threads[n];

    if (!prepare_thread_array(thread_list, threads, n))
        return NULL;

    if (n != this->distances_part->n)
    {
        PyErr_SetString(PyExc_ValueError, "Wrong number of threads provided");
        return NULL;
    }

    sr_distances_part_compute(this->distances_part, threads);
    Py_RETURN_NONE;
}
