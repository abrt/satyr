AC_INIT([btparser], [0.18], [kklic@redhat.com])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl get rid of glib
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.21])

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS   | sed 's/-O[[0-9]] *//'`])

AM_PATH_PYTHON
if test -z "$PYTHON"
then
    echo "*** Essential program python not found" 1>&2
    exit 1
fi

# Just PKG_CHECK_MODULES([PYTHON], [python]) works only with python2.7+
# Below, if python is not found, we set up for python2.6 w/o checking:
PKG_CHECK_MODULES([PYTHON], [python],,[
    PYTHON_LIBS='-L/usr/lib64 -lpython2.6'
    PYTHON_CFLAGS='-I/usr/include/python2.6'
])

# Fingerprinting -- x86_64 only
AC_ARG_ENABLE([fingerprints],
        AS_HELP_STRING([--enable-fingerprints], [enable assembly-based function fingerprinting (x86_64 only)]),
        [enable_fingerprints=$enableval], [enable_fingerprints=yes])
AC_CANONICAL_HOST
AM_CONDITIONAL(ENABLE_DISASSEMBLY, [test "$host_cpu" = "x86_64" -a "$enable_fingerprints" != "no"])

AC_CONFIG_FILES([
	btparser.spec
	btparser.pc
	Makefile
	lib/Makefile
	python/Makefile
])

AC_OUTPUT
