INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LE_LIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor_common.la

libextractor_common_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS)

libextractor_common_la_LIBADD = \
  $(LTLIBICONV) -lz

libextractor_common_la_SOURCES = \
  unzip.c \
  unzip.h \
  pack.c \
  pack.h \
  convert.c \
  convert.h \
  convert_numeric.c \
  convert_numeric.h
