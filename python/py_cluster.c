#include "py_cluster.h"
#include "py_metrics.h"
#include "lib/strbuf.h"
#include "lib/cluster.h"

#define dendrogram_doc "btparser.Distances - class representing distances between a set of objects\n"

#define de_get_size_doc "Usage: dendrogram.get_size()\n" \
                        "Returns: integer - number of objects in the dendrogram"

#define de_get_object_doc "Usage: dendrogram.get_object(i)\n" \
                          "Returns: integer - index of the object at position i"

#define de_get_merge_level_doc "Usage: dendrogram.get_merge_level(i)\n" \
                               "Returns: float - merge level between clusters at positions i and i + 1"

#define de_cut_doc "Usage: dendrogram.cut(level, min_size)\n" \
                   "Returns: list of clusters (lists of objects) which have at least min_size objects\n" \
                   "and which were merged at most at the specified distance"

static PyMethodDef
dendrogram_methods[] =
{
    /* getters & setters */
    { "get_size",        btp_py_dendrogram_get_size,        METH_NOARGS,  de_get_size_doc        },
    { "get_object",      btp_py_dendrogram_get_object,      METH_VARARGS, de_get_object_doc      },
    { "get_merge_level", btp_py_dendrogram_get_merge_level, METH_VARARGS, de_get_merge_level_doc },
    /* methods */
    { "cut",             btp_py_dendrogram_cut,             METH_VARARGS, de_cut_doc             },
    { NULL },
};

PyTypeObject
btp_py_dendrogram_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "btparser.Dendrogram",      /* tp_name */
    sizeof(struct btp_py_dendrogram),   /* tp_basicsize */
    0,                          /* tp_itemsize */
    btp_py_dendrogram_free,      /* tp_dealloc */
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
    btp_py_dendrogram_str,       /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    dendrogram_doc,             /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    dendrogram_methods,          /* tp_methods */
    NULL,                       /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    btp_py_dendrogram_new,       /* tp_new */
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
btp_py_dendrogram_new(PyTypeObject *object,
                      PyObject *args,
                      PyObject *kwds)
{
    struct btp_py_dendrogram *o = (struct btp_py_dendrogram*)
        PyObject_New(struct btp_py_dendrogram, &btp_py_dendrogram_type);

    if (!o)
        return PyErr_NoMemory();

    struct btp_py_distances *distances;

    if (!PyArg_ParseTuple(args, "O!", &btp_py_distances_type, &distances))
        return NULL;

    o->dendrogram = btp_distances_cluster_objects(distances->distances);

    return (PyObject*)o;
}

/* destructor */
void
btp_py_dendrogram_free(PyObject *object)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)object;
    btp_dendrogram_free(this->dendrogram);
    PyObject_Del(object);
}

PyObject *
btp_py_dendrogram_str(PyObject *self)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)self;
    struct btp_strbuf *buf = btp_strbuf_new();
    btp_strbuf_append_strf(buf, "Dendrogram with %d objects",
                           this->dendrogram->size);
    char *str = btp_strbuf_free_nobuf(buf);
    PyObject *result = Py_BuildValue("s", str);
    free(str);
    return result;
}

/* getters & setters */
PyObject *
btp_py_dendrogram_get_size(PyObject *self, PyObject *args)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)self;
    return Py_BuildValue("i", this->dendrogram->size);
}

PyObject *
btp_py_dendrogram_get_object(PyObject *self, PyObject *args)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)self;
    int i;
    if (!PyArg_ParseTuple(args, "i", &i))
        return NULL;

    if (i < 0 || i >= this->dendrogram->size)
    {
        PyErr_SetString(PyExc_ValueError, "Object position out of range");
        return NULL;
    }

    return Py_BuildValue("i", this->dendrogram->order[i]);
}

PyObject *
btp_py_dendrogram_get_merge_level(PyObject *self, PyObject *args)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)self;
    int i;
    if (!PyArg_ParseTuple(args, "i", &i))
        return NULL;

    if (i < 0 || i + 1 >= this->dendrogram->size)
    {
        PyErr_SetString(PyExc_ValueError, "Merge level position out of range");
        return NULL;
    }

    return Py_BuildValue("f", this->dendrogram->merge_levels[i]);
}

/* methods */
PyObject *
btp_py_dendrogram_cut(PyObject *self, PyObject *args)
{
    struct btp_py_dendrogram *this = (struct btp_py_dendrogram*)self;
    float level;
    int min_size, i;

    if (!PyArg_ParseTuple(args, "fi", &level, &min_size))
        return NULL;

    struct btp_cluster *cl, *cluster = btp_dendrogram_cut(this->dendrogram,
                                                          level, min_size);

    PyObject *list = PyList_New(0), *listc;

    while (cluster)
    {
        listc = PyList_New(0);
        for (i = 0; i < cluster->size; i++)
            PyList_Append(listc, PyInt_FromLong(cluster->objects[i]));
        PyList_Append(list, listc);

        cl = cluster;
        cluster = cluster->next;
        btp_cluster_free(cl);
    }

    return list;
}
