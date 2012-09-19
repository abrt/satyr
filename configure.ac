AC_INIT([btparser], [0.20], [kklic@redhat.com])

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

AC_PATH_PROG([PYTHON], [python-config], [no])
[if test "$XMLTO" = "no"]
[then]
    [echo "The python-config program was not found in the search path. Please ensure"]
    [echo "that it is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build libreport."]
    [exit 1]
[fi]

PYTHON_CFLAGS=`python-config --cflags 2> /dev/null`
PYTHON_LIBS=`python-config --libs 2> /dev/null`
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)


# Fingerprinting -- x86_64 only
AC_MSG_CHECKING([whether libopcodes is position independent])
opcodes_pic="not found"
# ugly, but still better than whitelisting known-working systems
for DIR in /lib /usr/lib /lib64 /usr/lib64; do
    OPCODES="$DIR/libopcodes.a"
    if ! test -r $OPCODES; then
        continue
    fi

    if readelf -r $OPCODES | grep R_X86_64_32 >/dev/null; then
        opcodes_pic=no
        break
    else
        opcodes_pic=yes
        break
    fi
done
AC_MSG_RESULT([$opcodes_pic])

AC_ARG_ENABLE([fingerprints],
        AS_HELP_STRING([--enable-fingerprints], [enable assembly-based function fingerprinting (x86_64 only)]),
        [enable_fingerprints=$enableval], [enable_fingerprints=yes])
AC_CANONICAL_HOST
AM_CONDITIONAL(ENABLE_DISASSEMBLY, [test "$host_cpu" = "x86_64" -a "$enable_fingerprints" != "no" -a "$opcodes_pic" = "yes"])

AC_CONFIG_FILES([
	btparser.spec
	btparser.pc
	Makefile
	lib/Makefile
	python/Makefile
])

AC_OUTPUT
