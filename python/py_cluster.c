#include "py_common.h"
#include "py_cluster.h"
#include "py_metrics.h"
#include "cluster.h"
#include <glib.h>

#define dendrogram_doc "satyr.Dendrogram - a dendrogram created by clustering algorithm\n\n" \
                       "Usage: satyr.Dendrogram(distances) - creates new dendrogram from a distance matrix"

#define de_get_size_doc "Usage: dendrogram.get_size()\n\n" \
                        "Returns: integer - number of objects in the dendrogram"

#define de_get_object_doc "Usage: dendrogram.get_object(i)\n\n" \
                          "Returns: integer - index of the object at position i"

#define de_get_merge_level_doc "Usage: dendrogram.get_merge_level(i)\n\n" \
                               "Returns: float - merge level between clusters at positions i and i + 1"

#define de_cut_doc "Usage: dendrogram.cut(level, min_size)\n\n" \
                   "Returns: list of clusters (lists of objects) which have at least min_size objects\n" \
                   "and which were merged at most at the specified distance"

static PyMethodDef
dendrogram_methods[] =
{
    /* getters & setters */
    { "get_size",        sr_py_dendrogram_get_size,        METH_NOARGS,  de_get_size_doc        },
    { "get_object",      sr_py_dendrogram_get_object,      METH_VARARGS, de_get_object_doc      },
    { "get_merge_level", sr_py_dendrogram_get_merge_level, METH_VARARGS, de_get_merge_level_doc },
    /* methods */
    { "cut",             sr_py_dendrogram_cut,             METH_VARARGS, de_cut_doc             },
    { NULL },
};

PyTypeObject
sr_py_dendrogram_type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "satyr.Dendrogram",      /* tp_name */
    sizeof(struct sr_py_dendrogram),   /* tp_basicsize */
    0,                          /* tp_itemsize */
    sr_py_dendrogram_free,      /* tp_dealloc */
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
    sr_py_dendrogram_str,       /* tp_str */
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
    sr_py_dendrogram_new,       /* tp_new */
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
sr_py_dendrogram_new(PyTypeObject *object,
                     PyObject *args,
                     PyObject *kwds)
{
    struct sr_py_dendrogram *o = (struct sr_py_dendrogram*)
        PyObject_New(struct sr_py_dendrogram, &sr_py_dendrogram_type);

    if (!o)
        return PyErr_NoMemory();

    struct sr_py_distances *distances;

    if (!PyArg_ParseTuple(args, "O!", &sr_py_distances_type, &distances))
        return NULL;

    o->dendrogram = sr_distances_cluster_objects(distances->distances);

    return (PyObject*)o;
}

/* destructor */
void
sr_py_dendrogram_free(PyObject *object)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)object;
    sr_dendrogram_free(this->dendrogram);
    PyObject_Del(object);
}

PyObject *
sr_py_dendrogram_str(PyObject *self)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)self;
    GString *buf = g_string_new(NULL);
    g_string_append_printf(buf, "Dendrogram with %d objects",
                          this->dendrogram->size);
    char *str = g_string_free(buf, FALSE);
    PyObject *result = Py_BuildValue("s", str);
    g_free(str);
    return result;
}

/* getters & setters */
PyObject *
sr_py_dendrogram_get_size(PyObject *self, PyObject *args)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)self;
    return Py_BuildValue("i", this->dendrogram->size);
}

PyObject *
sr_py_dendrogram_get_object(PyObject *self, PyObject *args)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)self;
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
sr_py_dendrogram_get_merge_level(PyObject *self, PyObject *args)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)self;
    int i;
    if (!PyArg_ParseTuple(args, "i", &i))
        return NULL;

    if (i < 0 || i > this->dendrogram->size)
    {
        PyErr_SetString(PyExc_ValueError, "Merge level position out of range");
        return NULL;
    }

    return Py_BuildValue("f", this->dendrogram->merge_levels[i]);
}

/* methods */
PyObject *
sr_py_dendrogram_cut(PyObject *self, PyObject *args)
{
    struct sr_py_dendrogram *this = (struct sr_py_dendrogram*)self;
    float level;
    int min_size, i;

    if (!PyArg_ParseTuple(args, "fi", &level, &min_size))
        return NULL;

    struct sr_cluster *cl, *cluster = sr_dendrogram_cut(this->dendrogram,
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
        sr_cluster_free(cl);
    }

    return list;
}
