INCLUDES = -I$(top_srcdir)/src/include

EXTRA_DIST = SYMBOLS

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

PLUGINFLAGS = -Wl,-Bsymbolic -avoid-version -module -no-undefined

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_oo.la

libextractor_oo_la_SOURCES = \
  ooextractor.c

libextractor_oo_la_LDFLAGS = \
 $(PLUGINFLAGS) -Wl,--retain-symbols-file -Wl,SYMBOLS

libextractor_oo_la_LIBADD = \
 $(top_builddir)/src/main/libextractor.la \
 -lz 
