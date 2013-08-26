clientexecdir = $(pyexecdir)/satyr

clientexec_PYTHON = \
    __init__.py

clientexec_LTLIBRARIES = _satyr.la

_satyr_la_SOURCES = \
    py_cluster.h \
    py_cluster.c \
    py_base_frame.h \
    py_base_frame.c \
    py_base_thread.h \
    py_base_thread.c \
    py_base_stacktrace.h \
    py_base_stacktrace.c \
    py_core_frame.h \
    py_core_frame.c \
    py_core_thread.h \
    py_core_thread.c \
    py_core_stacktrace.h \
    py_core_stacktrace.c \
    py_gdb_frame.h \
    py_gdb_frame.c \
    py_gdb_sharedlib.h \
    py_gdb_sharedlib.c \
    py_gdb_stacktrace.h \
    py_gdb_stacktrace.c \
    py_gdb_thread.h \
    py_gdb_thread.c \
    py_koops_frame.h \
    py_koops_frame.c \
    py_koops_stacktrace.h \
    py_koops_stacktrace.c \
    py_python_frame.h \
    py_python_frame.c \
    py_python_stacktrace.h \
    py_python_stacktrace.c \
    py_java_frame.h \
    py_java_frame.c \
    py_java_thread.h \
    py_java_thread.c \
    py_java_stacktrace.h \
    py_java_stacktrace.c \
    py_metrics.h \
    py_metrics.c \
    py_operating_system.h \
    py_operating_system.c \
    py_common.h \
    py_common.c \
    py_module.c

_satyr_la_CPPFLAGS = \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -I../include \
    -I../lib \
    $(PYTHON_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -fno-strict-aliasing

_satyr_la_LDFLAGS = \
    -module \
    -avoid-version \
    -export-symbols-regex init_satyr \
    ../lib/libsatyr.la
