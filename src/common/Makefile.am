INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LE_LIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor_common.la

libextractor_common_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) \
  -version-info 1:0:0

libextractor_common_la_LIBADD = \
  $(LTLIBICONV) -lz

libextractor_common_la_SOURCES = \
  le_architecture.h \
  unzip.c unzip.h \
  convert.c convert.h 
