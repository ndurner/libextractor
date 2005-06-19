INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_oo.la

libextractor_oo_la_SOURCES = \
  crypt.h \
  ioapi.c \
  ioapi.h \
  unzip.c \
  unzip.h \
  ooextractor.c

libextractor_oo_la_LDFLAGS = \
 -export-dynamic -avoid-version -module

libextractor_oo_la_LIBADD = \
 -lz 
