noinst_PROGRAMS = \
	dump_core

dump_core_SOURCES = dump_core.c
dump_core_LDFLAGS = -static

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_CFLAGS) \
	-I$(abs_top_srcdir)/include
AM_LDFLAGS = \
	$(GLIB_LIBS)
LDADD = \
	$(abs_top_builddir)/lib/libsatyr.la

check_PROGRAMS = \
	abrt \
	cluster \
	core_frame \
	core_stacktrace \
	core_thread \
	gdb_sharedlib \
	java_frame \
	java_stacktrace \
	java_thread \
	koops_frame \
	metrics \
	normalize \
	operating_system \
	report \
	rpm \
	ruby_frame

abrt_SOURCES = abrt.c
cluster_SOURCES = cluster.c
core_frame_SOURCES = core_frame.c
EXTRA_core_stacktrace_DEPENDENCIES = dump_core
core_stacktrace_SOURCES = core_stacktrace.c
core_thread_SOURCES = core_thread.c
gdb_sharedlib_SOURCES = gdb_sharedlib.c
java_frame_SOURCES = java_frame.c
java_stacktrace_SOURCES = java_stacktrace.c
java_thread_SOURCES = java_thread.c
koops_frame_SOURCES = koops_frame.c
metrics_SOURCES = metrics.c
normalize_SOURCES = normalize.c
operating_system_SOURCES = operating_system.c
report_SOURCES = report.c
rpm_SOURCES = rpm.c
ruby_frame_SOURCES = ruby_frame.c

TESTS = $(check_PROGRAMS)

EXTRA_DIST = gdb_stacktraces \
             java_stacktraces \
             ruby_stacktraces \
             js_stacktraces \
             json_files \
             kerneloopses \
             programs \
             python/satyr.py \
             python/core.py \
             python/gdb.py \
             python/java.py \
             python/koops.py \
             python/metrics.py \
             python/misc.py \
             python/python.py \
             python/report.py \
             python/ruby.py \
             python/javascript.py \
             python/test_helpers.py \
             python_stacktraces \
             problem_dir

## ------------ ##
## package.m4.  ##
## ------------ ##

package.m4: Makefile.in
	{ \
             echo '# Signature of the current package.' && \
             echo 'm4_define([AT_PACKAGE_NAME],' && \
             echo '  [@PACKAGE_NAME@])' && \
             echo 'm4_define([AT_PACKAGE_TARNAME],' && \
             echo '  [@PACKAGE_TARNAME@])' && \
             echo 'm4_define([AT_PACKAGE_VERSION],' && \
             echo '  [@PACKAGE_VERSION@])' && \
             echo 'm4_define([AT_PACKAGE_STRING],' && \
             echo '  [@PACKAGE_STRING@])' && \
             echo 'm4_define([AT_PACKAGE_BUGREPORT],' && \
             echo '  [@PACKAGE_BUGREPORT@])'; \
             echo 'm4_define([AT_PACKAGE_URL],' && \
             echo '  [@PACKAGE_URL@])'; \
	} >'package.m4'
EXTRA_DIST += package.m4

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT =		\
  local.at 		\
  testsuite.at		\
  utils.at 		\
  gdb_frame.at 		\
  gdb_thread.at 	\
  gdb_stacktrace.at  	\
  koops_stacktrace.at   \
  ruby_stacktrace.at	\
  js_platform.at \
  js_frame.at \
  js_stacktrace.at \
  python_bindings.at

EXTRA_DIST += $(TESTSUITE_AT)
TESTSUITE = $(srcdir)/testsuite
EXTRA_DIST += $(TESTSUITE)
MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE)
check_DATA = atconfig atlocal $(TESTSUITE)
DISTCLEANFILES = atconfig
EXTRA_DIST += atlocal.in
EXTRA_DIST += java_testcases.c

TESTSUITEFLAGS =

if !WITH_PYTHON3
TESTSUITEFLAGS += "SKIP_PYTHON3=yes"
endif

atconfig: $(top_builddir)/config.status
	(cd ${top_builddir} && ./config.status ${subdir}/atconfig)

check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' $(TESTSUITEFLAGS) ||:

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(TESTSUITE_AT) $(srcdir)/package.m4
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@
