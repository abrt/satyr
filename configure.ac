AC_INIT([satyr], m4_esyscmd([cat ./satyr-version]), [crash-catcher@fedorahosted.org])
AC_CANONICAL_TARGET
AC_CONFIG_HEADERS([lib/config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE
PKG_PROG_PKG_CONFIG

# Support silent build rules. Disable by either passing --disable-silent-rules
# to ./configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
# The 'yes' argument is undocumented but it is common knowledge that it works.
AM_SILENT_RULES([yes])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AC_ARG_ENABLE(doxygen-docs,
    AS_HELP_STRING([--enable-doxygen-docs],
    [build DOXYGEN documentation (requires Doxygen)]),
    [enable_doxygen_docs=$enableval],
    [enable_doxygen_docs=no]
)

# Doxygen Documentation

AC_PATH_PROG(DOXYGEN, doxygen, no)

AC_MSG_CHECKING([whether to build Doxygen documentation])

if test "$DOXYGEN" = "no" ; then
    have_doxygen=no
else
    have_doxygen=yes
fi

if test "$enable_doxygen_docs" = "yes" ; then
    if test "$have_doxygen" = "no"; then
        AC_MSG_ERROR([Building Doxygen docs explicitly required, but Doxygen not found])
    fi
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(DOXYGEN_DOCS_ENABLED, test "$enable_doxygen_docs" = "yes")

AC_PATH_PROG([GPERF], [gperf], [no])
if test "x$GPERF" = "xno"
then
    AC_MSG_ERROR(["gperf not found in the search path."])
fi

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([O0CFLAGS], [`echo $CFLAGS | sed 's/-O[[0-9]] *//'`])

AC_ARG_ENABLE([static-python-module],
              [AS_HELP_STRING([--enable-static-python-module],
                              [Compile the python module so that it does not load libsatyr.so
                               dynamically but includes the object files directly.
                               Might be needed to use the module on OpenShift.])],
              [enable_static_python_module=$enableval],
              [enable_static_python_module=no])
AM_CONDITIONAL(ENABLE_STATIC_PYTHON_MODULE, [test $enable_static_python_module = yes])

AC_ARG_WITH([python3],
            [AS_HELP_STRING([--without-python3],
                            [Build without python3 support.])],
            [with_python3=$withval],
            [with_python3=yes])
AM_CONDITIONAL(WITH_PYTHON3, [test $with_python3 = yes])
[if test "$with_python3" = "yes"]
[then]
    AM_PATH_PYTHON([3.6],,
                   [AC_MSG_ERROR([
    The python3 program was not found in the search path. Please ensure
    that it is installed and its directory is included in the search path or
    pass --without-python3 to ./configure.
    Then run configure again before attempting to build satyr.
                                 ])])

    AC_PATH_PROG([PYTHON3_CONFIG], [python${PYTHON_VERSION}-config], [no])
    [if test "$PYTHON3_CONFIG" = "no"]
    [then]
        [echo "The python${PYTHON_VERSION}-config program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-python3 to ./configure."]
        [echo "Then run configure again before attempting to build satyr."]
        [exit 1]
    [fi]

    PYTHON3_CFLAGS=`${PYTHON3_CONFIG} --cflags 2> /dev/null`
    PYTHON3_LIBS=`${PYTHON3_CONFIG} --libs 2> /dev/null`

    AC_SUBST([PYTHON3_CFLAGS])
    AC_SUBST([PYTHON3_LIBS])
    AC_SUBST([python3dir], [$pythondir])
    AC_SUBST([py3execdir], [$pyexecdir])
[fi]

# Check BFD
AC_CHECK_LIB([bfd], [main])
AC_CHECK_LIB([dl], [main])

PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([JSON], [json-c])
PKG_CHECK_MODULES([NETTLE], [nettle])

PKG_CHECK_MODULES([LIBDW], [libdw], [AC_DEFINE([HAVE_LIBDW], [], [libdw found])])
PKG_CHECK_MODULES([LIBELF], [libelf], [AC_DEFINE([HAVE_LIBELF], [], [libelf found])])

AC_ARG_WITH([unwinder],
            [AS_HELP_STRING([--with-unwinder=@<:@elfutils/libunwind@:>@],
                            [call stack unwinder to use @<:@default=elfutils@:>@])],
            [],
            [with_unwinder=elfutils])
AS_IF([test "x$with_unwinder" = "xlibunwind"],
      [PKG_CHECK_MODULES([LIBUNWIND],
                         [libunwind-coredump],
                         [AC_DEFINE([WITH_LIBUNWIND], [], [use libunwind for unwinding])])],
      [AC_DEFINE([WITH_LIBDWFL], [], [use elfutils for unwinding])])

# rpm
AC_ARG_WITH([rpm],
            [AS_HELP_STRING([--with-rpm],
                            [Build with rpm support.])],
            [with_rpm=$withval],
            [with_rpm=yes])
[if test "$with_rpm" = "yes"]
[then]
    PKG_CHECK_MODULES([RPM], [rpm])
    AC_DEFINE(HAVE_LIBRPM, [], [Have rpm support.])
[fi]

# c++ symbol demangling
AC_CHECK_LIB([stdc++], [__cxa_demangle], [], [echo "error: stdc++ library not found"; exit 1])

AC_CHECK_PROGS([VALGRIND], [valgrind])

AC_CONFIG_FILES([
	satyr.pc
	Makefile
	apidoc/Makefile
	apidoc/Doxyfile
	apidoc/html/Makefile
	lib/Makefile
	python/Makefile
	python/doc/Makefile
	tests/Makefile
	include/Makefile
	tests/atlocal
        ruby/Makefile
])

AC_OUTPUT
