ACLOCAL_AMFLAGS = -I build-aux/m4

AM_CXXFLAGS = -std=c++03 -fpic -g3 @CFLAGS@
AM_CPPFLAGS = -std=c++03 -fpic -g3 @CFLAGS@

AM_CFLAGS = --fpic -g3 @CFLAGS@

lib_LTLIBRARIES = libmcl.la

include_HEADERS = include/mcl/bn.h include/mcl/bn_c384_256.h

libmcl_la_SOURCES = src/fp.cpp src/bn_c384_256.cpp
libmcl_la_LDFLAGS = -version-info $(LIB_VERSION_CURRENT):$(LIB_VERSION_REVISION):$(LIB_VERSION_AGE) @LDFLAGS@

noinst_DATA = src/bint$(BIT).lo src/base$(BIT).lo src/msm_avx.lo
libmcl_la_LIBADD = $(noinst_DATA)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmcl.pc

src/msm_avx.lo: src/msm_avx.cpp
		$(LIBTOOL) --mode=compile $(CC) -c $< -o $@ @CFLAGS@ -mavx512f -mavx512ifma -std=c++11 -fno-exceptions -fno-rtti -DCYBOZU_DONT_USE_STRING -DCYBOZU_DONT_USE_EXCEPTION -DNDEBUG

src/bint64.lo: src/asm/bint-x64-amd64.S
		$(LIBTOOL) --mode=compile $(CC) -c $< -o $@ @CFLAGS@

src/base64.lo: src/asm/x86-64.S
		$(LIBTOOL) --mode=compile $(CC) -c $< -o $@ @CFLAGS@

