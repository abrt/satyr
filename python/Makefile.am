SUBDIRS = . doc

py2satyrdir = $(pyexecdir)/satyr

py2satyr_PYTHON = \
    __init__.py

py2satyr_LTLIBRARIES = _satyr.la

PYEXTFILES = \
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
    py_ruby_frame.h \
    py_ruby_frame.c \
    py_ruby_stacktrace.h \
    py_ruby_stacktrace.c \
    py_js_frame.h \
    py_js_frame.c \
    py_js_stacktrace.h \
    py_js_stacktrace.c \
    py_rpm_package.h \
    py_rpm_package.c \
    py_metrics.h \
    py_metrics.c \
    py_operating_system.h \
    py_operating_system.c \
    py_report.h \
    py_report.c \
    py_common.h \
    py_common.c \
    py_module.c

PYEXTCPPFLAGS = \
    -I../include \
    -I../lib \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -fno-strict-aliasing

PYEXTLDFLAGS = \
    -module \
    -avoid-version

_satyr_la_SOURCES = $(PYEXTFILES)
_satyr_la_CPPFLAGS = \
    $(PYEXTCPPFLAGS) \
    $(PYTHON_CFLAGS)
_satyr_la_LDFLAGS = \
    $(PYEXTLDFLAGS) \
    -export-symbols-regex init_satyr

# Depending on the configure option, we either link libsatyr.so dynamically (as
# is usual), or directly include its object files (by including the
# convenience library).
if ENABLE_STATIC_PYTHON_MODULE
_satyr_la_LIBADD = ../lib/libsatyr_conv.la
else
_satyr_la_LIBADD = ../lib/libsatyr.la
endif

if WITH_PYTHON3
py3satyrdir = $(py3execdir)/satyr

# XXX: Using _PYTHON instead of _DATA runs py-compile that produces python2
# .pyc and .pyo files, which we obviously don't want. Treat it as data for now,
# RPM macros will produce the bytecode anyway.
py3satyr_DATA = \
    __init__.py

py3satyr_LTLIBRARIES = _satyr3.la

_satyr3_la_SOURCES = $(PYEXTFILES)
_satyr3_la_CPPFLAGS = \
    $(PYEXTCPPFLAGS) \
    $(PYTHON3_CFLAGS) \
    -Wno-error=declaration-after-statement
_satyr3_la_LDFLAGS = \
    $(PYEXTLDFLAGS) \
    -export-symbols-regex PyInit__satyr3

if ENABLE_STATIC_PYTHON_MODULE
_satyr3_la_LIBADD = ../lib/libsatyr_conv.la
else
_satyr3_la_LIBADD = ../lib/libsatyr.la
endif
endif
