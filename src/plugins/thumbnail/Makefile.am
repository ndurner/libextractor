INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_thumbnail.la

libextractor_thumbnail_la_SOURCES = \
  thumbnailextractor.c

libextractor_thumbnail_la_LDFLAGS = \
 -export-dynamic -avoid-version -module @WAND_LDFLAGS@

libextractor_thumbnail_la_CFLAGS = \
 @WAND_CFLAGS@ @CFLAGS@

libextractor_thumbnail_la_LIBADD = \
 @WAND_LIBS@
