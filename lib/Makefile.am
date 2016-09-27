# NOTE: First, we build so-called 'convenience library' from all sources. IIUC,
# convenience libraries are something like collection of object files and they
# are only used at build time, never installed. We then use it to create
# libsatyr.so (and possibly ../python/_satyr.so).
#
# This is needed in order to build satyr python module that does NOT load
# libsatyr.so dynamically in order to circumvent OpenShift limitation.
#
# See following page for description of convenience libraries:
# https://sourceware.org/autobook/autobook/autobook_92.html

noinst_LTLIBRARIES = libsatyr_conv.la

libsatyr_conv_la_SOURCES = \
	callgraph.h \
	cluster.h \
	disasm.h \
	elves.h \
	sha1.h \
	unstrip.h \
	abrt.c \
	callgraph.c \
	cluster.c \
	core_stacktrace.c \
	core_fingerprint.c \
	core_frame.c \
	core_thread.c \
	core_unwind.c \
	core_unwind_elfutils.c \
	core_unwind_libunwind.c \
	deb.c \
	disasm.c \
	distance.c \
	elves.c \
	generic_stacktrace.c \
	generic_stacktrace.h \
	generic_thread.c \
	generic_thread.h \
	generic_frame.c \
	generic_frame.h \
	gdb_stacktrace.c \
	gdb_frame.c \
	gdb_sharedlib.c \
	gdb_thread.c \
	internal_utils.h \
	internal_unwind.h \
	java_frame.c \
	java_thread.c \
	java_stacktrace.c \
	json.c \
	koops_frame.c \
	koops_stacktrace.c \
	location.c \
	normalize.c \
	operating_system.c \
	python_frame.c \
	python_stacktrace.c \
	report.c \
	rpm.c \
	ruby_frame.c \
	ruby_stacktrace.c \
	js_platform.c \
	js_frame.c \
	js_stacktrace.c \
	sha1.c \
	strbuf.c \
	unstrip.c \
	utils.c

libsatyr_conv_la_CFLAGS = -Wall -Wformat=2 -std=gnu99 -D_GNU_SOURCE -I$(top_srcdir)/include $(GLIB_CFLAGS)
libsatyr_conv_la_LDFLAGS = $(GLIB_LIBS)

lib_LTLIBRARIES = libsatyr.la
libsatyr_la_SOURCES = 
libsatyr_la_LIBADD = libsatyr_conv.la
libsatyr_la_LDFLAGS = -version-info 3:0:0 -export-symbols-regex '^sr_'

# NOTE: when updating CURRENT, update it in ruby/lib/satyr.rb as well!

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
