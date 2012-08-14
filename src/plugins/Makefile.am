INCLUDES = \
 -I$(top_srcdir)/src/include \
 -I$(top_srcdir)/src/common \
 -I$(top_srcdir)/src/main

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
  testdata/jpeg_image.jpg \
  testdata/rpm_test.rpm \
  testdata/flac_kraftwerk.flac \
  testdata/mpeg_alien.mpg \
  testdata/mpeg_melt.mpg \
  testdata/wav_noise.wav \
  testdata/wav_alert.wav \
  testdata/it_dawn.it \
  testdata/s3m_2nd_pm.s3m \
  testdata/ole2_msword.doc \
  testdata/ole2_starwriter40.sdw \
  testdata/ole2_blair.doc \
  testdata/ole2_excel.xls \
  testdata/png_image.png \
  testdata/odf_cg.odt \
  testdata/deb_bzip2.deb

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

if HAVE_JPEG
PLUGIN_JPEG=libextractor_jpeg.la
TEST_JPEG=test_jpeg
endif

if HAVE_EXIV2
PLUGIN_EXIV2=libextractor_exiv2.la
TEST_EXIV2=test_exiv2
endif

if HAVE_GSF
PLUGIN_GSF=libextractor_ole2.la
TEST_GSF=test_ole2
endif

if HAVE_ZLIB
PLUGIN_ZLIB=libextractor_deb.la
TEST_ZLIB=test_deb
endif

plugin_LTLIBRARIES = \
  libextractor_it.la \
  libextractor_nsf.la \
  libextractor_nsfe.la \
  libextractor_odf.la \
  libextractor_png.la \
  libextractor_xm.la \
  libextractor_s3m.la \
  libextractor_sid.la \
  libextractor_wav.la \
  libextractor_zip.la \
  $(PLUGIN_ZLIB) \
  $(PLUGIN_OGG) \
  $(PLUGIN_MIME) \
  $(PLUGIN_GIF) \
  $(PLUGIN_RPM) \
  $(PLUGIN_FLAC) \
  $(PLUGIN_MPEG) \
  $(PLUGIN_JPEG) \
  $(PLUGIN_EXIV2) \
  $(PLUGIN_GSF)

if HAVE_ZZUF
  fuzz_tests=fuzz_default.sh 
endif

check_PROGRAMS = \
  test_wav \
  test_it \
  test_s3m \
  test_png \
  test_odf \
  test_zip \
  $(TEST_ZLIB) \
  $(TEST_OGG) \
  $(TEST_MIME) \
  $(TEST_GIF) \
  $(TEST_RPM) \
  $(TEST_FLAC) \
  $(TEST_MPEG) \
  $(TEST_JPEG) \
  $(TEST_EXIV2) \
  $(TEST_GSF)

TESTS = \
  $(fuzz_tests) \
  $(check_PROGRAMS)


noinst_LTLIBRARIES = \
  libtest.la

libtest_la_SOURCES = \
  test_lib.c 
libtest_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la


libextractor_xm_la_SOURCES = \
  xm_extractor.c
libextractor_xm_la_LDFLAGS = \
  $(PLUGINFLAGS)


libextractor_deb_la_SOURCES = \
  deb_extractor.c
libextractor_deb_la_LDFLAGS = \
  $(PLUGINFLAGS) -lz

test_deb_SOURCES = \
  test_deb.c
test_deb_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_nsf_la_SOURCES = \
  nsf_extractor.c
libextractor_nsf_la_LDFLAGS = \
  $(PLUGINFLAGS)


libextractor_nsfe_la_SOURCES = \
  nsfe_extractor.c
libextractor_nsfe_la_LDFLAGS = \
  $(PLUGINFLAGS)


libextractor_odf_la_SOURCES = \
  odf_extractor.c
libextractor_odf_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_odf_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la 

test_odf_SOURCES = \
  test_odf.c
test_odf_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_zip_la_SOURCES = \
  zip_extractor.c
libextractor_zip_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_zip_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la 

test_zip_SOURCES = \
  test_zip.c
test_zip_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_png_la_SOURCES = \
  png_extractor.c
libextractor_png_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la 

test_png_SOURCES = \
  test_png.c
test_png_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_it_la_SOURCES = \
  it_extractor.c
libextractor_it_la_LDFLAGS = \
  $(PLUGINFLAGS)

test_it_SOURCES = \
  test_it.c
test_it_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_sid_la_SOURCES = \
  sid_extractor.c
libextractor_sid_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_s3m_la_SOURCES = \
  s3m_extractor.c
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)

test_s3m_SOURCES = \
  test_s3m.c
test_s3m_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_wav_la_SOURCES = \
  wav_extractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)

test_wav_SOURCES = \
  test_wav.c
test_wav_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


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


libextractor_jpeg_la_SOURCES = \
  jpeg_extractor.c
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_jpeg_la_LIBADD = \
  -ljpeg

test_jpeg_SOURCES = \
  test_jpeg.c
test_jpeg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_exiv2_la_SOURCES = \
  exiv2_extractor.cc
libextractor_exiv2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_exiv2_la_LIBADD = \
  -lexiv2

test_exiv2_SOURCES = \
  test_exiv2.c
test_exiv2_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_ole2_la_SOURCES = \
  ole2_extractor.c
libextractor_ole2_la_CFLAGS = \
  $(GSF_CFLAGS)
libextractor_ole2_la_CPPFLAGS = \
  $(GSF_CFLAGS)
libextractor_ole2_la_LDFLAGS = \
  $(PLUGINFLAGS) 
libextractor_ole2_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(GSF_LIBS)

test_ole2_SOURCES = \
  test_ole2.c
test_ole2_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


