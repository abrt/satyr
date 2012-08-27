#include <Python.h>
#include <structmember.h>

#include "gdb_backtrace.h"
#include "doc.h"
#include "gdb_frame.h"
#include "location.h"
#include "strbuf.h"
#include "gdb_thread.h"
#include "metrics.h"
#include "cluster.h"
#include "gdb_sharedlib.h"
#include "normalize.h"
#include "utils.h"

/*********/
/* frame */
/*********/

PyTypeObject FrameTypeObject;

typedef struct {
    PyObject_HEAD
    struct btp_gdb_frame *frame;
} FrameObject;

/* constructor */
PyObject *p_btp_gdb_frame_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_gdb_frame_free(PyObject *object);

/* str */
PyObject *p_btp_gdb_frame_str(PyObject *self);

/* getters & setters */
PyObject *p_btp_gdb_frame_get_function_name(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_function_name(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_function_type(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_function_type(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_number(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_number(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_source_file(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_source_file(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_source_line(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_source_line(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_signal_handler_called(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_signal_handler_called(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_address(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_address(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_get_library_name(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_set_library_name(PyObject *self, PyObject *args);

/* methods */
PyObject *p_btp_gdb_frame_dup(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_cmp(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_calls_func(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_frame_calls_func_in_file(PyObject *self, PyObject *args);

/**********/
/* thread */
/**********/

PyTypeObject ThreadTypeObject;

typedef struct {
    PyObject_HEAD
    PyObject *frames;
    struct btp_gdb_thread *thread;
} ThreadObject;

/* helpers */
int thread_prepare_linked_list(ThreadObject *thread);
PyObject *frame_linked_list_to_python_list(struct btp_gdb_thread *thread);
int thread_rebuild_python_list(ThreadObject *thread);

/* constructor */
PyObject *p_btp_gdb_thread_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_gdb_thread_free(PyObject *object);

/* str */
PyObject *p_btp_gdb_thread_str(PyObject *self);

/* getters & setters */
PyObject *p_btp_gdb_thread_get_number(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_thread_set_number(PyObject *self, PyObject *args);

/* methods */
PyObject *p_btp_gdb_thread_dup(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_thread_cmp(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_thread_quality_counts(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_thread_quality(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_thread_format_funs(PyObject *self, PyObject *args);

/*************/
/* backtrace */
/*************/

PyTypeObject BacktraceTypeObject;

typedef struct {
    PyObject_HEAD
    struct btp_gdb_backtrace *backtrace;
    PyObject *threads;
    FrameObject *crashframe;
    ThreadObject *crashthread;
    PyObject *libs;
} BacktraceObject;

/* helpers */
int backtrace_prepare_linked_list(BacktraceObject *backtrace);
PyObject *backtrace_prepare_thread_list(struct btp_gdb_backtrace *backtrace);

/* constructor */
PyObject *p_btp_gdb_backtrace_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_gdb_backtrace_free(PyObject *object);

/* str */
PyObject *p_btp_gdb_backtrace_str(PyObject *self);

/* methods */
PyObject *p_btp_gdb_backtrace_dup(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_find_crash_frame(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_find_crash_thread(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_limit_frame_depth(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_quality_simple(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_quality_complex(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_get_duplication_hash(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_find_address(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_set_libnames(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_normalize(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_backtrace_get_optimized_thread(PyObject *self, PyObject *args);

/*************/
/* distances */
/*************/

extern PyTypeObject DistancesTypeObject;

typedef struct {
    PyObject_HEAD
    struct btp_distances *distances;
} DistancesObject;

/* constructor */
PyObject *p_btp_distances_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_distances_free(PyObject *object);

/* str */
PyObject *p_btp_distances_str(PyObject *self);

/* getters & setters */
PyObject *p_btp_distances_get_size(PyObject *self, PyObject *args);
PyObject *p_btp_distances_get_distance(PyObject *self, PyObject *args);
PyObject *p_btp_distances_set_distance(PyObject *self, PyObject *args);

/* methods */
PyObject *p_btp_distances_dup(PyObject *self, PyObject *args);


/**************/
/* dendrogram */
/**************/

extern PyTypeObject DendrogramTypeObject;

typedef struct {
    PyObject_HEAD
    struct btp_dendrogram *dendrogram;
} DendrogramObject;

/* constructor */
PyObject *p_btp_dendrogram_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_dendrogram_free(PyObject *object);

/* str */
PyObject *p_btp_dendrogram_str(PyObject *self);

/* getters & setters */
PyObject *p_btp_dendrogram_get_size(PyObject *self, PyObject *args);
PyObject *p_btp_dendrogram_get_object(PyObject *self, PyObject *args);
PyObject *p_btp_dendrogram_get_merge_level(PyObject *self, PyObject *args);

/* methods */
PyObject *p_btp_dendrogram_cut(PyObject *self, PyObject *args);

/*************/
/* Sharedlib */
/*************/

extern PyTypeObject SharedlibTypeObject;

typedef struct {
    PyObject_HEAD
    struct btp_gdb_sharedlib *sharedlib;
    int syms_ok;
    int syms_wrong;
    int syms_not_found;
} SharedlibObject;

/* constructor */
PyObject *p_btp_gdb_sharedlib_new(PyTypeObject *object, PyObject *args, PyObject *kwds);

/* destructor */
void p_btp_gdb_sharedlib_free(PyObject *object);

/* str */
PyObject *p_btp_gdb_sharedlib_str(PyObject *self);

/* getters & setters */
PyObject *p_btp_gdb_sharedlib_get_from(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_set_from(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_get_to(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_set_to(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_get_soname(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_set_soname(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_get_symbols(PyObject *self, PyObject *args);
PyObject *p_btp_gdb_sharedlib_set_symbols(PyObject *self, PyObject *args);
