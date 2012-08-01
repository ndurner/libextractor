INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/common -I$(top_srcdir)/src/main

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

SUBDIRS = . 

if HAVE_VORBISFILE
 PLUGIN_OGG=libextractor_ogg.la
endif

#plugin_LTLIBRARIES = \
#  libextractor_id3.la \
#  libextractor_id3v2.la \
#  libextractor_ebml.la \
#  libextractor_s3m.la \
#  $(PLUGIN_OGG) \
#  libextractor_png.la \
#  libextractor_mp3.la

libextractor_mp3_la_SOURCES = \
  mp3_extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mp3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(top_builddir)/src/main/libextractor.la \
  $(LE_LIBINTL)

libextractor_ebml_la_SOURCES = \
  ebml_extractor.c 
libextractor_ebml_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)

libextractor_id3_la_SOURCES = \
  id3_extractor.c 
libextractor_id3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(top_builddir)/src/main/libextractor.la \
  $(LE_LIBINTL)

libextractor_id3v2_la_SOURCES = \
  id3v2_extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v2_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_s3m_la_SOURCES = \
  s3m_extractor.c
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_s3m_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_ogg_la_SOURCES = \
  ogg_extractor.c
libextractor_ogg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ogg_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/common/libextractor_common.la \
  -lvorbisfile -lvorbis $(vorbisflag) -logg

libextractor_png_la_SOURCES = \
  png_extractor.c
libextractor_png_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/common/libextractor_common.la

EXTRA_DIST = template_extractor.c 

if HAVE_ZZUF
  fuzz_tests=fuzz_default.sh fuzz_thumbnail.sh
endif

TESTS = $(fuzz_tests)