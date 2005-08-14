INCLUDES = \
  -I$(top_srcdir)/src/include \
  @GTK_CFLAGS@ 

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
  $(PLUGINFLAGS) 
libextractor_thumbnail_la_SOURCES = \
  thumbnailextractor.c
