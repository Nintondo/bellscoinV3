AC_PREREQ([2.69])

define(_PKG_VERSION_MAJOR, 0)
define(_PKG_VERSION_MINOR, 1)
define(_PKG_VERSION_BUILD, 0)
define(_PKG_VERSION_IS_RELEASE, false)

define(_LIB_VERSION_CURRENT, 1)
define(_LIB_VERSION_REVISION, 93)
define(_LIB_VERSION_AGE, 0)
AC_INIT([libmcl], m4_join([.], _PKG_VERSION_MAJOR, _PKG_VERSION_MINOR, _PKG_VERSION_BUILD)m4_if(_PKG_VERSION_IS_RELEASE, [true], [], [-pre]), [https://github.com/herumi/mcl/issues], [libmcl], [https://github.com/herumi/mcl])
OBJ_DIR=obj
AC_SUBST(OBJ_DIR)
AC_SUBST(LIB_VERSION_CURRENT, _LIB_VERSION_CURRENT)
AC_SUBST(LIB_VERSION_REVISION, _LIB_VERSION_REVISION)
AC_SUBST(LIB_VERSION_AGE, _LIB_VERSION_AGE)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CANONICAL_HOST
AH_TOP([#ifndef LIBMCL_CONFIG_H])
AH_TOP([#define LIBMCL_CONFIG_H])
AH_BOTTOM([#endif /*LIBMCL_CONFIG_H*/])

AM_INIT_AUTOMAKE([1.11.2 foreign subdir-objects])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR

LT_INIT([win32-dll])

AC_CANONICAL_HOST
case "$host_os" in
    *mingw64*)
        AC_DEFINE([MINGW64], [1], [Define to 1 if on MinGW-W64])
        CFLAGS="$CFLAGS -D__USE_MINGW_ANSI_STDIO=1"
        LDFLAGS="$LDFLAGS -lrt"
        ;;
    *cygwin*)
        AC_DEFINE([CYGWIN], [1], [Define if on Cygwin])
        ;;
    *darwin*)
        AC_DEFINE([DARWIN], [1], [Define if on Darwin/Mac OS X])
        LDFLAGS="$LDFLAGS -lrt"
        ;;
    *openbsd*)
        AC_DEFINE([OPENBSD], [1], [Define if on OpenBSD])
        CFLAGS="$CFLAGS -I/usr/local/include"
        LDFLAGS="$LDFLAGS -lrt -L/usr/local/lib"
        ;;
    *freebsd*)
        AC_DEFINE([FREEBSD], [1], [Define if on FreeBSD])
        CFLAGS="$CFLAGS -I/usr/local/include"
        LDFLAGS="$LDFLAGS -L/usr/local/lib"
        ;;
    *linux*)
        AC_DEFINE([LINUX], [1], [Define if on Linux])
        CFLAGS="$CFLAGS -I/usr/local/include"
        LDFLAGS="$LDFLAGS -L/usr/local/lib"
        ;;
esac
case "$host_cpu" in
    x86_64)
        AC_DEFINE([ARCH_X86_64], [1], [Define if on x86_64])
        BIT=64
        BIT_OPT=-m64
        ;;
    i386|i486|i586|i686)
        AC_DEFINE([ARCH_X86], [1], [Define if on x86])
        BIT=32
        BIT_OPT=-m32
        ;;
    arm*)
        AC_DEFINE([ARCH_ARM], [1], [Define if on ARM])
        BIT=32
        ;;
    aarch64)
        AC_DEFINE([ARCH_AARCH64], [1], [Define if on AArch64])
        BIT=64
        ;;
esac

MCL_SIZEOF_UNIT=`expr $BIT / 8`
MCL_MAX_BIT_SIZE=384

CFLAGS_WARN="-Wall -Wextra -Wformat=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith -Wundef"
MCL_CFLAGS="-DMCL_NO_AUTOLINK -DMCLBN_NO_AUTOLINK -DMCL_USE_LLVM=1 -DMCL_SIZEOF_UNIT=$MCL_SIZEOF_UNIT -DMCL_MAX_BIT_SIZE=$MCL_MAX_BIT_SIZE"
CFLAGS="$CFLAGS $BIT_OPT $CFLAGS_WARN $MCL_CFLAGS -I include -I src"
LDFLAGS="$LDFLAGS $BIT_OPT"

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(BIT)
AC_SUBST(CXX)
AC_SUBST(CC)
AC_SUBST(MCL_MAX_BIT_SIZE)

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([Makefile libmcl.pc])

AC_OUTPUT

echo
echo "  target os     = $TARGET_OS"
echo "  build os      = $BUILD_OS"
echo
echo "  CC            = $CC"
echo "  CFLAGS        = $CFLAGS"
echo "  CPPFLAGS      = $CPPFLAGS"
echo "  CXX           = $CXX"
echo "  CXXFLAGS      = $CXXFLAGS"
echo "  LDFLAGS       = $LDFLAGS"
echo
