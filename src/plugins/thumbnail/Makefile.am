INCLUDES = \
  -I$(top_srcdir)/src/include \
  @GTK_CFLAGS@ 

EXTRA_DIST = SYMBOLS

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

PLUGINFLAGS = -export-dynamic -avoid-version -module

plugin_LTLIBRARIES = \
 libextractor_thumbnail.la

AM_CFLAGS = $(GLIB_CFLAGS)

libextractor_thumbnail_la_CFLAGS = \
  $(GLIB_CFLAGS)
libextractor_thumbnail_la_LIBADD = \
  $(LIBADD) -lgobject-2.0 @GTK_LIBS@ \
  $(top_builddir)/src/main/libextractor.la 
libextractor_thumbnail_la_LDFLAGS = \
  $(PLUGINFLAGS)  -Wl,--retain-symbols-file -Wl,SYMBOLS 
libextractor_thumbnail_la_SOURCES = \
  thumbnailextractor.c
