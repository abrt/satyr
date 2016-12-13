AC_INIT([satyr], m4_esyscmd([cat ./satyr-version]), [crash-catcher@fedorahosted.org])
AC_CANONICAL_TARGET
AC_CONFIG_HEADERS([lib/config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

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
    [echo "Then run configure again before attempting to build Satyr."]
[fi]

# Just PKG_CHECK_MODULES([PYTHON], [python]) works only with python2.7+
# Below, if python is not found, we set up for python2.6 w/o checking:
PKG_CHECK_MODULES([PYTHON], [python2],,[
    PYTHON_LIBS='-L/usr/lib64 -lpython2.6'
    PYTHON_CFLAGS='-I/usr/include/python2.6'
])

AC_ARG_ENABLE([python-manpage],
              [AS_HELP_STRING([--disable-python-manpage],
                              [Disable python bindings manpage build])],
              [enable_python_manpage=$enableval],
              [enable_python_manpage=yes])
AM_CONDITIONAL(ENABLE_PYTHON_MANPAGE, [test $enable_python_manpage = yes])

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
    AC_PATH_PROG([PYTHON3], [python3], [no])
    [if test "$PYTHON3" == "no"]
    [then]
        [echo "The python3 program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-python3 to ./configure."]
        [echo "Then run configure again before attempting to build satyr."]
        [exit 1]
    [fi]

    AC_PATH_PROG([PYTHON3_CONFIG], [python3-config], [no])
    [if test "$PYTHON3_CONFIG" = "no"]
    [then]
        [echo "The python3-config program was not found in the search path. Please ensure"]
        [echo "that it is installed and its directory is included in the search path or"]
        [echo "pass --without-python3 to ./configure."]
        [echo "Then run configure again before attempting to build satyr."]
        [exit 1]
    [fi]

    PYTHON3_CFLAGS=`python3-config --cflags 2> /dev/null`
    PYTHON3_LIBS=`python3-config --libs 2> /dev/null`

    AC_SUBST([PYTHON3_PREFIX], ['${prefix}'])
    AC_SUBST([PYTHON3_EXEC_PREFIX], ['${exec_prefix}'])

    PYTHON3_DIR=`$PYTHON3 -c "import distutils.sysconfig; \
        print(distutils.sysconfig.get_python_lib(0,0,prefix='$PYTHON3_PREFIX'))"`
    PYTHON3_EXECDIR=`$PYTHON3 -c "import distutils.sysconfig; \
        print(distutils.sysconfig.get_python_lib(1,0,prefix='$PYTHON3_EXEC_PREFIX'))"`

    AC_SUBST(PYTHON3_CFLAGS)
    AC_SUBST(PYTHON3_LIBS)
    AC_SUBST(python3dir, $PYTHON3_DIR)
    AC_SUBST(py3execdir, $PYTHON3_EXECDIR)
[fi]

# Check BFD
AC_CHECK_LIB([bfd], [main])

# elfutils
AC_CHECK_HEADERS([dwarf.h elfutils/libdw.h elfutils/libdwfl.h gelf.h libelf.h])
AC_CHECK_LIB([elf], [main])
# dwfl is actually part of libdw, at least on Fedora
AC_CHECK_LIB([dw], [main])
AC_CHECK_LIB([dwfl], [main])
AC_CHECK_LIB([dl], [main])
elfutils_unwinder=0
AC_CHECK_FUNC(dwfl_getthreads,
  AC_DEFINE(HAVE_DWFL_NEXT_THREAD, [], [Have function dwfl_getthreads for coredump unwinding])
  elfutils_unwinder=1
)

# libunwind
if test "$elfutils_unwinder" != "1"; then
  AC_CHECK_HEADERS([libunwind-coredump.h])
  AC_CHECK_LIB([unwind], [main])
  AC_CHECK_LIB([unwind-generic], [main])
  AC_CHECK_LIB([unwind-coredump], [main])
fi

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

AC_CONFIG_FILES([
	satyr.pc
	Makefile
	lib/Makefile
	python/Makefile
	python/doc/Makefile
	tests/Makefile
	include/Makefile
	tests/atlocal
        ruby/Makefile
])

AC_OUTPUT
