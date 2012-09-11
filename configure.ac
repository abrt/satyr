AC_INIT([btparser], [0.18], [kklic@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS | sed 's/-O[[0-9]] *//'`])

AM_PATH_PYTHON
AM_CONDITIONAL([HAVE_PYTHON], test -n "$PYTHON")
[if test -z "$PYTHON"; then]
    [echo "The python interpreter was not found in the search path. The Python language bindings"]
    [echo "will not be built.  If you want to build the Python language bindings, please ensure"]
    [echo "that python is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

# Just PKG_CHECK_MODULES([PYTHON], [python]) works only with python2.7+
# Below, if python is not found, we set up for python2.6 w/o checking:
PKG_CHECK_MODULES([PYTHON], [python2],,[
    PYTHON_LIBS='-L/usr/lib64 -lpython2.6'
    PYTHON_CFLAGS='-I/usr/include/python2.6'
])

# Fingerprinting -- x86_64 only
AC_ARG_ENABLE([fingerprints],
        AS_HELP_STRING([--enable-fingerprints], [enable assembly-based function fingerprinting (x86_64 only)]),
        [enable_fingerprints=$enableval], [enable_fingerprints=yes])
AC_CANONICAL_HOST
AM_CONDITIONAL(ENABLE_DISASSEMBLY, [test "$host_cpu" = "x86_64" -a "$enable_fingerprints" != "no"])

# pdflatex
AC_PATH_PROG([PDFLATEX], [pdflatex], [no])
AM_CONDITIONAL([HAVE_PDFLATEX], test "$PDFLATEX" != "no")
[if test "$PDFLATEX" = "no"; then]
    [echo "The pdflatex program was not found in the search path. The documentation"]
    [echo "will not be built.  If you want to build the documentation, please ensure"]
    [echo "that pdflatex is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

# doxygen
AC_PATH_PROG([DOXYGEN], [doxygen], [no])
AM_CONDITIONAL([HAVE_DOXYGEN], test "$DOXYGEN" != "no")
[if test "$DOXYGEN" = "no"; then]
    [echo "The doxygen program was not found in the search path. The documentation"]
    [echo "will not be built.  If you want to build the documentation, please ensure"]
    [echo "that doxygen is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
    [exit 1]
[fi]

# dot (part of graphviz
AC_PATH_PROG([DOT], [dot], [no])
AM_CONDITIONAL([HAVE_DOT], test "$DOT" != "no")
[if test "DOT" = "no"; then]
    [echo "The dot program was not found in the search path. The documentation"]
    [echo "will not be built.  If you want to build the documentation, please ensure"]
    [echo "that dot is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

AC_CONFIG_FILES([
	btparser.spec
	btparser.pc
	Makefile
	lib/Makefile
	python/Makefile
	docs/Makefile
	tests/Makefile
	tests/atlocal
])

AC_OUTPUT
