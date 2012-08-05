INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/common -I$(top_srcdir)/src/main

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

SUBDIRS = . 

EXTRA_DIST = template_extractor.c \
  testdata/ogg_courseclear.ogg \
  testdata/gif_image.gif \
  testdata/rpm_test.rpm \
  testdata/flac_kraftwerk.flac

if HAVE_VORBISFILE
PLUGIN_OGG=libextractor_ogg.la
TEST_OGG=test_ogg
endif

if HAVE_MAGIC
PLUGIN_MIME=libextractor_mime.la
TEST_MIME=test_mime
endif

if HAVE_GIF
PLUGIN_GIF=libextractor_gif.la
TEST_GIF=test_gif
endif

if HAVE_LIBRPM
PLUGIN_RPM=libextractor_rpm.la
TEST_RPM=test_rpm
endif

if HAVE_FLAC
PLUGIN_FLAC=libextractor_flac.la
TEST_FLAC=test_flac
endif

if HAVE_MPEG2
PLUGIN_MPEG=libextractor_mpeg.la
TEST_MPEG=test_mpeg
endif


plugin_LTLIBRARIES = \
  $(PLUGIN_OGG) \
  $(PLUGIN_MIME) \
  $(PLUGIN_GIF) \
  $(PLUGIN_RPM) \
  $(PLUGIN_FLAC) \
  $(PLUGIN_MPEG)

if HAVE_ZZUF
  fuzz_tests=fuzz_default.sh 
endif

check_PROGRAMS = \
  $(TEST_OGG) \
  $(TEST_MIME) \
  $(TEST_GIF) \
  $(TEST_RPM) \
  $(TEST_FLAC) \
  $(TEST_MPEG)

TESTS = \
  $(fuzz_tests) \
  $(check_PROGRAMS)


noinst_LTLIBRARIES = \
  libtest.la

libtest_la_SOURCES = \
  test_lib.c 
libtest_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la



libextractor_ogg_la_SOURCES = \
  ogg_extractor.c
libextractor_ogg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ogg_la_LIBADD = \
  -lvorbisfile -lvorbis $(vorbisflag) -logg

test_ogg_SOURCES = \
  test_ogg.c
test_ogg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_mime_la_SOURCES = \
  mime_extractor.c
libextractor_mime_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mime_la_LIBADD = \
  -lmagic

test_mime_SOURCES = \
  test_mime.c
test_mime_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_gif_la_SOURCES = \
  gif_extractor.c
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_gif_la_LIBADD = \
  -lgif

test_gif_SOURCES = \
  test_gif.c
test_gif_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_rpm_la_SOURCES = \
  rpm_extractor.c
libextractor_rpm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_rpm_la_LIBADD = \
  -lrpm -lpthread

test_rpm_SOURCES = \
  test_rpm.c
test_rpm_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_flac_la_SOURCES = \
  flac_extractor.c
libextractor_flac_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_flac_la_LIBADD = \
  -lFLAC

test_flac_SOURCES = \
  test_flac.c
test_flac_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_mpeg_la_SOURCES = \
  mpeg_extractor.c
libextractor_mpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mpeg_la_LIBADD = \
  -lmpeg2

test_mpeg_SOURCES = \
  test_mpeg.c
test_mpeg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


