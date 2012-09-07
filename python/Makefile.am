clientexecdir = $(pyexecdir)/btparser

clientexec_PYTHON = \
    __init__.py

clientexec_LTLIBRARIES = _btparser.la

_btparser_la_SOURCES = \
    py_cluster.c \
    py_gdb_frame.c \
    py_gdb_sharedlib.c \
    py_gdb_stacktrace.c \
    py_gdb_thread.c \
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
