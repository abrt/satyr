clientexecdir = $(pyexecdir)/btparser

clientexec_PYTHON = \
    __init__.py

clientexec_LTLIBRARIES = _btparser.la

_btparser_la_SOURCES = \
    module.c \
    frame.c \
    thread.c \
    backtrace.c \
    dendrogram.c \
    distances.c \
    sharedlib.c \
    common.h doc.h
_btparser_la_CPPFLAGS = \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -I../lib/ \
    $(PYTHON_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -fno-strict-aliasing
_btparser_la_LDFLAGS = \
    -module \
    -avoid-version \
    -export-symbols-regex init_btparser \
    ../lib/libbtparser.la
