clientexecdir = $(pyexecdir)/satyr

clientexec_PYTHON = \
    __init__.py

clientexec_LTLIBRARIES = _btparser.la

_btparser_la_SOURCES = \
    py_cluster.h \
    py_cluster.c \
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
    py_module.c

_btparser_la_CPPFLAGS = \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -I.. \
    $(PYTHON_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -fno-strict-aliasing

_btparser_la_LDFLAGS = \
    -module \
    -avoid-version \
    -export-symbols-regex init_btparser \
    ../lib/libbtparser.la
