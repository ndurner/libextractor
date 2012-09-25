INCLUDES = \
 -I$(top_srcdir)/src/include \
 -I$(top_srcdir)/src/common 


# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

SUBDIRS = . 

EXTRA_DIST = \
  fuzz_default.sh \
  template_extractor.c \
  testdata/archive_test.tar \
  testdata/deb_bzip2.deb \
  testdata/dvi_ora.dvi \
  testdata/exiv2_iptc.jpg \
  testdata/flac_kraftwerk.flac \
  testdata/gif_image.gif \
  testdata/gstreamer_30_and_33.asf \
  testdata/gstreamer_barsandtone.flv \
  testdata/gstreamer_sample_sorenson.mov \
  testdata/html_grothoff.html \
  testdata/it_dawn.it \
  testdata/jpeg_image.jpg \
  testdata/man_extract.1 \
  testdata/matroska_flame.mkv \
  testdata/midi_dth.mid \
  testdata/mpeg_alien.mpg \
  testdata/mpeg_melt.mpg \
  testdata/nsf_arkanoid.nsf \
  testdata/nsfe_classics.nsfe \
  testdata/odf_cg.odt \
  testdata/ole2_blair.doc \
  testdata/ole2_excel.xls \
  testdata/ole2_msword.doc \
  testdata/ole2_starwriter40.sdw \
  testdata/ogg_courseclear.ogg \
  testdata/png_image.png \
  testdata/ps_bloomfilter.ps \
  testdata/ps_wallace.ps \
  testdata/riff_flame.avi \
  testdata/rpm_test.rpm \
  testdata/s3m_2nd_pm.s3m \
  testdata/sid_wizball.sid \
  testdata/thumbnail_torsten.jpg \
  testdata/tiff_haute.tiff \
  testdata/wav_noise.wav \
  testdata/wav_alert.wav \
  testdata/xm_diesel.xm \
  testdata/zip_test.zip


if HAVE_MAGIC
PLUGIN_MIME=libextractor_mime.la
TEST_MIME=test_mime

if HAVE_FFMPEG
# FFmpeg-thumbnailer requires MAGIC and FFMPEG
PLUGIN_FFMPEG=libextractor_thumbnailffmpeg.la
TEST_FFMPEG=test_thumbnailffmpeg
endif

if HAVE_GTK
# Gtk-thumbnailer requires MAGIC and GTK
PLUGIN_GTK=libextractor_thumbnailgtk.la
TEST_GTK=test_thumbnailgtk
endif

if HAVE_TIDY
# HTML requires MAGIC and tidy
PLUGIN_HTML=libextractor_html.la
TEST_HTML=test_html
endif
endif


if HAVE_ARCHIVE
PLUGIN_ARCHIVE=libextractor_archive.la
TEST_ARCHIVE=test_archive
endif

if HAVE_EXIV2
PLUGIN_EXIV2=libextractor_exiv2.la
TEST_EXIV2=test_exiv2
endif

if HAVE_FLAC
PLUGIN_FLAC=libextractor_flac.la
TEST_FLAC=test_flac
endif

if HAVE_GIF
PLUGIN_GIF=libextractor_gif.la
TEST_GIF=test_gif
endif

if HAVE_GSF
PLUGIN_GSF=libextractor_ole2.la
TEST_GSF=test_ole2
endif

if HAVE_GSTREAMER
if HAVE_EXPERIMENTAL
PLUGIN_GSTREAMER=libextractor_gstreamer.la
TEST_GSTREAMER=test_gstreamer
endif
endif

if HAVE_JPEG
PLUGIN_JPEG=libextractor_jpeg.la
TEST_JPEG=test_jpeg
endif

if HAVE_MP4
if HAVE_EXPERIMENTAL
PLUGIN_MP4=libextractor_mp4.la
TEST_MP4=test_mp4
endif
endif

if HAVE_MPEG2
PLUGIN_MPEG=libextractor_mpeg.la
TEST_MPEG=test_mpeg
endif

if HAVE_LIBRPM
PLUGIN_RPM=libextractor_rpm.la
TEST_RPM=test_rpm
endif

if HAVE_SMF
PLUGIN_MIDI=libextractor_midi.la
TEST_MIDI=test_midi
endif

if HAVE_TIFF
PLUGIN_TIFF=libextractor_tiff.la
TEST_TIFF=test_tiff
endif

if HAVE_VORBISFILE
PLUGIN_OGG=libextractor_ogg.la
TEST_OGG=test_ogg
endif

if HAVE_ZLIB
PLUGIN_ZLIB=libextractor_deb.la
TEST_ZLIB=test_deb
endif


plugin_LTLIBRARIES = \
  libextractor_dvi.la \
  libextractor_it.la \
  libextractor_man.la \
  libextractor_nsf.la \
  libextractor_nsfe.la \
  libextractor_odf.la \
  libextractor_png.la \
  libextractor_ps.la \
  libextractor_riff.la \
  libextractor_s3m.la \
  libextractor_sid.la \
  libextractor_wav.la \
  libextractor_xm.la \
  libextractor_zip.la \
  $(PLUGIN_ARCHIVE) \
  $(PLUGIN_EXIV2) \
  $(PLUGIN_FFMPEG) \
  $(PLUGIN_FLAC) \
  $(PLUGIN_GIF) \
  $(PLUGIN_GSF) \
  $(PLUGIN_GSTREAMER) \
  $(PLUGIN_GTK) \
  $(PLUGIN_HTML) \
  $(PLUGIN_JPEG) \
  $(PLUGIN_MIDI) \
  $(PLUGIN_MIME) \
  $(PLUGIN_MP4) \
  $(PLUGIN_MPEG) \
  $(PLUGIN_OGG) \
  $(PLUGIN_RPM) \
  $(PLUGIN_TIFF) \
  $(PLUGIN_ZLIB) 

if HAVE_ZZUF
  fuzz_tests=fuzz_default.sh 
endif

check_PROGRAMS = \
  test_dvi \
  test_it \
  test_man \
  test_nsf \
  test_nsfe \
  test_odf \
  test_ps \
  test_png \
  test_riff \
  test_s3m \
  test_sid \
  test_wav \
  test_xm \
  test_zip \
  $(TEST_ARCHIVE) \
  $(TEST_EXIV2) \
  $(TEST_FFMPEG) \
  $(TEST_FLAC) \
  $(TEST_GIF) \
  $(TEST_GSF) \
  $(TEST_GSTREAMER) \
  $(TEST_GTK) \
  $(TEST_HTML) \
  $(TEST_JPEG) \
  $(TEST_MIDI) \
  $(TEST_MIME) \
  $(TEST_MPEG) \
  $(TEST_OGG) \
  $(TEST_RPM) \
  $(TEST_TIFF) \
  $(TEST_ZLIB) 

if ENABLE_TEST_RUN
TESTS = \
  $(fuzz_tests) \
  $(check_PROGRAMS)
endif

noinst_LTLIBRARIES = \
  libtest.la

libtest_la_SOURCES = \
  test_lib.c test_lib.h
libtest_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la $(XLIB) 


libextractor_archive_la_SOURCES = \
  archive_extractor.c
libextractor_archive_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_archive_la_LIBADD = \
  -larchive $(XLIB) 

test_archive_SOURCES = \
  test_archive.c
test_archive_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_deb_la_SOURCES = \
  deb_extractor.c
libextractor_deb_la_LDFLAGS = \
  $(PLUGINFLAGS) -lz
libextractor_deb_la_LIBADD = \
 $(XLIB) 

test_deb_SOURCES = \
  test_deb.c
test_deb_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_dvi_la_SOURCES = \
  dvi_extractor.c
libextractor_dvi_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_dvi_la_LIBADD = \
 $(XLIB) 

test_dvi_SOURCES = \
  test_dvi.c
test_dvi_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_exiv2_la_SOURCES = \
  exiv2_extractor.cc
libextractor_exiv2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_exiv2_la_LIBADD = \
  -lexiv2 $(XLIB)

test_exiv2_SOURCES = \
  test_exiv2.c
test_exiv2_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_flac_la_SOURCES = \
  flac_extractor.c
libextractor_flac_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_flac_la_LIBADD = \
  -lFLAC $(XLIB) $(LE_LIBINTL) 

test_flac_SOURCES = \
  test_flac.c
test_flac_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_gif_la_SOURCES = \
  gif_extractor.c
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_gif_la_LIBADD = \
  -lgif $(XLIB)

test_gif_SOURCES = \
  test_gif.c
test_gif_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_gstreamer_la_SOURCES = \
  gstreamer_extractor.c
libextractor_gstreamer_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_gstreamer_la_LIBADD = \
  $(GSTREAMER_LIBS) $(GSTREAMER_PBUTILS_LIBS) $(GSTREAMER_TAG_LIBS) $(GSTREAMER_APP_LIBS) $(XLIB)
libextractor_gstreamer_la_CFLAGS = \
  $(GSTREAMER_CFLAGS) $(GSTREAMER_PBUTILS_CFLAGS) $(GSTREAMER_TAG_CFLAGS) $(GSTREAMER_APP_CFALGS) 

test_gstreamer_SOURCES = \
  test_gstreamer.c
test_gstreamer_LDADD = \
  $(top_builddir)/src/plugins/libtest.la \
  $(GSTREAMER_LIBS) $(GSTREAMER_PBUTILS_LIBS)
test_gstreamer_CFLAGS = \
  $(GSTREAMER_CFLAGS) $(GSTREAMER_PBUTILS_CFLAGS)


libextractor_html_la_SOURCES = \
  html_extractor.c
libextractor_html_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_html_la_LIBADD = \
  -ltidy -lmagic $(XLIB)

test_html_SOURCES = \
  test_html.c
test_html_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_it_la_SOURCES = \
  it_extractor.c
libextractor_it_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_it_la_LIBADD = \
 $(XLIB) 

test_it_SOURCES = \
  test_it.c
test_it_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_jpeg_la_SOURCES = \
  jpeg_extractor.c
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_jpeg_la_LIBADD = \
  -ljpeg $(XLIB)

test_jpeg_SOURCES = \
  test_jpeg.c
test_jpeg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_man_la_SOURCES = \
  man_extractor.c
libextractor_man_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_man_la_LIBADD = \
 $(XLIB) $(LE_LIBINTL) 

test_man_SOURCES = \
  test_man.c
test_man_LDADD = \
  $(top_builddir)/src/plugins/libtest.la \
  $(LE_LIBINTL) 


libextractor_midi_la_SOURCES = \
  midi_extractor.c
libextractor_midi_la_CFLAGS = \
  $(GLIB_CFLAGS)
libextractor_midi_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_midi_la_LIBADD = \
  -lsmf $(XLIB)

test_midi_SOURCES = \
  test_midi.c
test_midi_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_mime_la_SOURCES = \
  mime_extractor.c
libextractor_mime_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mime_la_LIBADD = \
  -lmagic $(XLIB)

test_mime_SOURCES = \
  test_mime.c
test_mime_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_mp4_la_SOURCES = \
  mp4_extractor.c
libextractor_mp4_la_LDFLAGS = \
  $(PLUGINFLAGS) 
libextractor_mp4_la_LIBADD = \
  -lmp4v2 $(XLIB)


libextractor_mpeg_la_SOURCES = \
  mpeg_extractor.c
libextractor_mpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mpeg_la_LIBADD = \
  -lmpeg2 $(XLIB)

test_mpeg_SOURCES = \
  test_mpeg.c
test_mpeg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_nsf_la_SOURCES = \
  nsf_extractor.c
libextractor_nsf_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_nsf_la_LIBADD = \
 $(XLIB) 

test_nsf_SOURCES = \
  test_nsf.c
test_nsf_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_nsfe_la_SOURCES = \
  nsfe_extractor.c
libextractor_nsfe_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_nsfe_la_LIBADD = \
 $(XLIB) 

test_nsfe_SOURCES = \
  test_nsfe.c
test_nsfe_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_odf_la_SOURCES = \
  odf_extractor.c
libextractor_odf_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_odf_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la $(XLIB)

test_odf_SOURCES = \
  test_odf.c
test_odf_LDADD = \
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
  $(GSF_LIBS) $(XLIB)  $(LE_LIBINTL) 

test_ole2_SOURCES = \
  test_ole2.c
test_ole2_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_ogg_la_SOURCES = \
  ogg_extractor.c
libextractor_ogg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ogg_la_LIBADD = \
  -lvorbisfile -lvorbis $(vorbisflag) -logg $(XLIB)

test_ogg_SOURCES = \
  test_ogg.c
test_ogg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_png_la_SOURCES = \
  png_extractor.c
libextractor_png_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la $(XLIB) -lz

test_png_SOURCES = \
  test_png.c
test_png_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_ps_la_SOURCES = \
  ps_extractor.c
libextractor_ps_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ps_la_LIBADD = \
 $(XLIB) 

test_ps_SOURCES = \
  test_ps.c
test_ps_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_riff_la_SOURCES = \
  riff_extractor.c
libextractor_riff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_riff_la_LIBADD = \
  -lm $(XLIB)  $(LE_LIBINTL) 


test_riff_SOURCES = \
  test_riff.c
test_riff_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_rpm_la_SOURCES = \
  rpm_extractor.c
libextractor_rpm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_rpm_la_LIBADD = \
  -lrpm -lpthread $(XLIB)

test_rpm_SOURCES = \
  test_rpm.c
test_rpm_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_s3m_la_SOURCES = \
  s3m_extractor.c
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_s3m_la_LIBADD = \
 $(XLIB) 

test_s3m_SOURCES = \
  test_s3m.c
test_s3m_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_sid_la_SOURCES = \
  sid_extractor.c
libextractor_sid_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_sid_la_LIBADD = \
 $(XLIB) 

test_sid_SOURCES = \
  test_sid.c
test_sid_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_thumbnailffmpeg_la_SOURCES = \
  thumbnailffmpeg_extractor.c
libextractor_thumbnailffmpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_thumbnailffmpeg_la_LIBADD = \
  -lavutil -lavformat -lavcodec -lswscale -lmagic $(XLIB)

test_thumbnailffmpeg_SOURCES = \
  test_thumbnailffmpeg.c
test_thumbnailffmpeg_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_thumbnailgtk_la_SOURCES = \
  thumbnailgtk_extractor.c
libextractor_thumbnailgtk_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_thumbnailgtk_la_CFLAGS = \
  $(GTK_CFLAGS)
libextractor_thumbnailgtk_la_LIBADD = \
  -lmagic $(GTK_LIBS) $(XLIB)

test_thumbnailgtk_SOURCES = \
  test_thumbnailgtk.c
test_thumbnailgtk_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_tiff_la_SOURCES = \
  tiff_extractor.c
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_tiff_la_LIBADD = \
  -ltiff $(XLIB)

test_tiff_SOURCES = \
  test_tiff.c
test_tiff_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_wav_la_SOURCES = \
  wav_extractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_wav_la_LIBADD = \
 $(XLIB) $(LE_LIBINTL) 

test_wav_SOURCES = \
  test_wav.c
test_wav_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_xm_la_SOURCES = \
  xm_extractor.c
libextractor_xm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_xm_la_LIBADD = \
 $(XLIB) 

test_xm_SOURCES = \
  test_xm.c
test_xm_LDADD = \
  $(top_builddir)/src/plugins/libtest.la


libextractor_zip_la_SOURCES = \
  zip_extractor.c
libextractor_zip_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_zip_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la $(XLIB)

test_zip_SOURCES = \
  test_zip.c
test_zip_LDADD = \
  $(top_builddir)/src/plugins/libtest.la

