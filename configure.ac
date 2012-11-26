AC_INIT([btparser], [1], [kklic@redhat.com])
AC_CONFIG_HEADERS([lib/config.h])
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

AC_CHECK_LIB([opcodes], [main], [have_libopcodes=yes], [have_libopcodes=no])
[if test "$have_libopcodes" = "no"; then]
    [echo "The libopcodes library was not found in the search path. The core stacktrace support "]
    [echo "will not be built.  If you want to build core stacktrace support, please ensure"]
    [echo "that libopcodes is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

# Check libopcodes
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

[if test "$opcodes_pic" = "no"; then]
    [echo "The libopcodes library on your system was not compiled with PIC. The core backtrace support "]
    [echo "will not be built.  If you want to build core backtrace support, please ensure"]
    [echo "that the provided libopcodes is build with -fPIC."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

test ! \( "$opcodes_pic" = "yes" -a "$have_libopcodes" = "yes" \)
have_good_libopcodes=$?
AM_CONDITIONAL(HAVE_LIBOPCODES, test $have_good_libopcodes -eq 1)
AC_DEFINE_UNQUOTED(HAVE_LIBOPCODES, $have_good_libopcodes, [Have libopcodes compiled with -fPIC])

# Check BFD
AC_CHECK_LIB([bfd], [main])

# elfutils
AC_CHECK_HEADERS([dwarf.h elfutils/libdw.h elfutils/libdwfl.h gelf.h libelf.h])
AC_CHECK_LIB([elf], [main])
# dwfl is actually part of libdw, at least on fedora
AC_CHECK_LIB([dw], [main])
AC_CHECK_LIB([dl], [main])

# libunwind
AC_CHECK_HEADERS([libunwind-coredump.h])
AC_CHECK_LIB([unwind], [main])
AC_CHECK_LIB([unwind-generic], [main])
AC_CHECK_LIB([unwind-coredump], [main])

# rpm
AC_CHECK_LIB([rpm], [main])

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
[fi]

# dot (part of graphviz)
AC_PATH_PROG([DOT], [dot], [no])
AM_CONDITIONAL([HAVE_DOT], test "$DOT" != "no")
[if test "DOT" = "no"; then]
    [echo "The dot program was not found in the search path. The documentation"]
    [echo "will not be built.  If you want to build the documentation, please ensure"]
    [echo "that dot is installed and its directory is included in the search path."]
    [echo "Then run configure again before attempting to build Btparser."]
[fi]

AM_CONDITIONAL([BUILD_DOCUMENTATION], test \( "$DOXYGEN" != "no" \) \
				      	-a \( "$DOT" != "no" \) \
					-a \( "$PDFLATEX" != "no" \))

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
