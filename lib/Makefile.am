pkginclude_HEADERS = \
	backtrace.h \
	cluster.h \
	frame.h \
	hash_sha1.h \
	location.h \
	metrics.h \
	normalize.h \
	sharedlib.h \
	strbuf.h \
	thread.h \
	utils.h \
	core-backtrace.h \
	core-backtrace-python.h

lib_LTLIBRARIES = libbtparser.la
libbtparser_la_SOURCES = \
	$(pkginclude_HEADERS) \
	core-backtrace.c \
	core-backtrace-fingerprints.c \
	core-backtrace-python.c \
	backtrace.c \
	cluster.c \
	frame.c \
	hash_sha1.c \
	location.c \
	metrics.c \
	normalize.c \
	normalize_dbus.c \
	normalize_gdk.c \
	normalize_glib.c \
	normalize_glibc.c \
	normalize_gtk.c \
	normalize_libstdcpp.c \
	normalize_linux.c \
	normalize_xorg.c \
	sharedlib.c \
	strbuf.c \
	thread.c \
	utils.c

libbtparser_la_CFLAGS = -Wall -D_GNU_SOURCE $(GLIB_CFLAGS)
libbtparser_la_LDFLAGS = -version-info 4:2:2 $(GLIB_LIBS)

if ENABLE_DISASSEMBLY
libbtparser_la_CFLAGS += -DENABLE_DISASSEMBLY
libbtparser_la_LIBADD = \
    -lelf \
    -ldw \
    -ldl \
    -lbfd \
    -lopcodes
endif

# From http://www.seul.org/docs/autotut/
# Version consists 3 numbers: CURRENT, REVISION, AGE.
# CURRENT is the version of the interface the library implements.
# Whenever a new function is added, or its name changed, or
# the number or type of its parameters (the prototype -- in
# libraries we call this the function signature), this number
# goes up. And it goes up exactly by one.
#
# REVISION is the revision of the implementation of this
# interface, i.e., when you change the library by only modifying
# code inside the functions (fixing bugs, optimizing internal
# behavior, or adding/removing/changing signatures of functions
# that are private to the library -- used only internally) you
# raise the revision number only.
#
# Age is the difference between the newest and oldest interface
# the library currently implements. Let's say you had 8 versions
# of your library's interface, 0 through 7. You are now on
# the 4th revision of the 8th interface, that is, 7:3:X (remember
# we start counting on zero). And when you had to make choices
# for what old interfaces you would keep support -- for backward
# compatibility purposes, you chose to keep support for
# interfaces 5, 6 and (obviously) the current, 7. The libtool
# version of your library would be 7:3:2 , because the Age
# is 7-5 = 2.
