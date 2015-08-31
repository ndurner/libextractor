# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

LIBS = \
 @LE_LIBINTL@ @LIBS@

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif
if HAVE_ZLIB
  zlib = -lz
  LINK_UNZIP = unzip.c unzip.h
endif

lib_LTLIBRARIES = \
  libextractor_common.la

libextractor_common_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) \
  -version-info 1:0:0

libextractor_common_la_LIBADD = \
  $(LTLIBICONV) $(zlib)

libextractor_common_la_SOURCES = \
  le_architecture.h \
  $(LINK_UNZIP) \
  convert.c convert.h
