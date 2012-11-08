pkginclude_HEADERS = \
	callgraph.h \
	cluster.h \
	config.h \
	core_fingerprint.h \
	core_frame.h \
	core_stacktrace.h \
	core_thread.h \
	core_unwind.h \
	disasm.h \
	elves.h \
	gdb_frame.h \
	gdb_sharedlib.h \
	gdb_stacktrace.h \
	gdb_thread.h \
	java_frame.h \
	java_thread.h \
	java_stacktrace.h \
	json.h \
	koops_frame.h \
	koops_stacktrace.h \
	location.h \
	metrics.h \
	normalize.h \
	python_frame.h \
	python_stacktrace.h \
	sha1.h \
	strbuf.h \
	unstrip.h \
	utils.h

lib_LTLIBRARIES = libbtparser.la
libbtparser_la_SOURCES = \
	$(pkginclude_HEADERS) \
	callgraph.c \
	cluster.c \
	core_stacktrace.c \
	core_fingerprint.c \
	core_frame.c \
	core_thread.c \
	core_unwind.c \
	disasm.c \
	elves.c \
	gdb_stacktrace.c \
	gdb_frame.c \
	gdb_sharedlib.c \
	gdb_thread.c \
	java_frame.c \
	java_thread.c \
	java_stacktrace.c \
	json.c \
	koops_frame.c \
	koops_stacktrace.c \
	location.c \
	metrics.c \
	normalize.c \
	python_frame.c \
	python_stacktrace.c \
	sha1.c \
	strbuf.c \
	unstrip.c \
	utils.c

libbtparser_la_CFLAGS = -Wall -std=gnu99 -D_GNU_SOURCE $(GLIB_CFLAGS)
libbtparser_la_LDFLAGS = -version-info 4:2:2 $(GLIB_LIBS)

if HAVE_LIBOPCODES
# Conditional link is required to avoid linking to a library without
# -fPIC.
libbtparser_la_LIBADD = -lopcodes
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
