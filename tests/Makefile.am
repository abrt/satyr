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
  strbuf.at		\
  gdb_frame.at 		\
  gdb_thread.at 	\
  gdb_stacktrace.at  	\
  gdb_sharedlib.at	\
  java_frame.at 	\
  java_thread.at 	\
  java_stacktrace.at 	\
  koops_frame.at	\
  koops_stacktrace.at   \
  operating_system.at   \
  core_frame.at         \
  core_thread.at        \
  core_stacktrace.at    \
  ruby_frame.at		\
  ruby_stacktrace.at	\
  js_platform.at \
  js_frame.at \
  js_stacktrace.at \
  normalize.at  	\
  metrics.at 		\
  cluster.at  		\
  rpm.at		\
  abrt.at               \
  report.at		\
  python_bindings.at

EXTRA_DIST += $(TESTSUITE_AT)
TESTSUITE = $(srcdir)/testsuite
EXTRA_DIST += $(TESTSUITE)
MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE)
check_DATA = atconfig atlocal $(TESTSUITE)
DISTCLEANFILES = atconfig
EXTRA_DIST += atlocal.in
EXTRA_DIST += java_testcases.c

if WITH_PYTHON3
TESTSUITEFLAGS =
else
TESTSUITEFLAGS = SKIP_PYTHON3=yes
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
