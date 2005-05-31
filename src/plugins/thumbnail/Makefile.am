INCLUDES = \
  -I$(top_srcdir)/src/include \
  @GTK_CFLAGS@ 

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

AM_CFLAGS = $(GLIB_CFLAGS)

plugin_LTLIBRARIES = \
 libextractor_thumbnail.la

libextractor_thumbnail_la_SOURCES = \
  thumbnailextractor.c

# Ok, linking this one is complicated, see Mantis #787.
libextractor_thumbnail_la_LDFLAGS = \
  -Wl,-Bstatic -Wl,-lgobject-2.0 -Wl,-lglib-2.0 -Wl,-Bdynamic \
  -Wl,-Bsymbolic -avoid-version -module

libextractor_thumbnail_la_LIBADD = \
 @GTK_LIBS@ 



