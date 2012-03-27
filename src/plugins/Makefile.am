INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/common -I$(top_srcdir)/src/main

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

SUBDIRS = . 

plugin_LTLIBRARIES = \
  libextractor_id3.la \
  libextractor_id3v2.la \
  libextractor_mp3.la

libextractor_mp3_la_SOURCES = \
  mp3_extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mp3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(LE_LIBINTL)

libextractor_ebml_la_SOURCES = \
  ebml_extractor.c 
libextractor_ebml_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_id3_la_SOURCES = \
  id3_extractor.c 
libextractor_id3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(LE_LIBINTL)

libextractor_id3v2_la_SOURCES = \
  id3v2_extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v2_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

EXTRA_DIST = template_extractor.c 
