INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/common

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

SUBDIRS = . 

if HAVE_FFMPEG
 thumbffmpeg=libextractor_thumbnailffmpeg.la
endif

if HAVE_LIBRPM
 rpm=libextractor_rpm.la 
endif

if HAVE_GLIB
if WITH_GSF
 ole2=libextractor_ole2.la
endif
if HAVE_GTK
 thumbgtk=libextractor_thumbnailgtk.la
endif
endif

if HAVE_QT
 thumbqt=libextractor_thumbnailqt.la
 qtflags=-lQtGui -lQtCore -lpthread
else
if HAVE_QT4
 thumbqt=libextractor_thumbnailqt.la
 qtflags=-lQtGui4 -lQtCore4
endif
endif

if HAVE_QT_SVG
 svgflags = -lQtSvg
else
if HAVE_QT_SVG4
 svgflags = -lQtSvg4
endif
endif

if HAVE_CXX
if HAVE_EXIV2
 exiv2=libextractor_exiv2.la
endif
if HAVE_POPPLER
 pdf=libextractor_pdf.la
endif
endif

if HAVE_MPEG2
 mpeg = libextractor_mpeg.la
endif

if HAVE_VORBISFILE
 ogg = libextractor_ogg.la
endif

if HAVE_FLAC
 flac = libextractor_flac.la
endif

if NEED_VORBIS
 vorbisflag = -lvorbis
endif

if NEED_OGG
 flacoggflag = -logg
endif

plugin_LTLIBRARIES = \
  libextractor_applefile.la \
  libextractor_asf.la \
  libextractor_deb.la \
  libextractor_dvi.la \
  libextractor_elf.la \
  $(exiv2) \
  $(flac) \
  libextractor_flv.la \
  libextractor_gif.la \
  libextractor_html.la \
  libextractor_id3.la \
  libextractor_id3v2.la \
  libextractor_id3v23.la \
  libextractor_id3v24.la \
  libextractor_it.la \
  libextractor_jpeg.la \
  libextractor_man.la \
  libextractor_mime.la \
  libextractor_mkv.la \
  libextractor_mp3.la \
  $(mpeg) \
  libextractor_nsf.la \
  libextractor_nsfe.la \
  libextractor_odf.la \
  $(ogg) \
  $(ole2) \
  $(pdf) \
  libextractor_png.la \
  libextractor_ps.la \
  libextractor_qt.la \
  libextractor_real.la \
  libextractor_riff.la \
  $(rpm) \
  libextractor_s3m.la \
  libextractor_sid.la \
  libextractor_tar.la \
  $(thumbgtk) \
  $(thumbqt) \
  $(thumbffmpeg) \
  libextractor_tiff.la \
  libextractor_wav.la \
  libextractor_xm.la \
  libextractor_zip.la

libextractor_applefile_la_SOURCES = \
  applefile_extractor.c
libextractor_applefile_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_applefile_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(LE_LIBINTL)

libextractor_asf_la_SOURCES = \
  asf_extractor.c 
libextractor_asf_la_LDFLAGS = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(PLUGINFLAGS)

libextractor_deb_la_SOURCES = \
  deb_extractor.c 
libextractor_deb_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_deb_la_LIBADD = \
  -lz

libextractor_dvi_la_SOURCES = \
  dvi_extractor.c 
libextractor_dvi_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_elf_la_SOURCES = \
  elf_extractor.c 
libextractor_elf_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_elf_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_exiv2_la_SOURCES = \
  exiv2_extractor.cc 
libextractor_exiv2_la_LDFLAGS = \
  $(XTRA_CPPLIBS) $(PLUGINFLAGS) 
libextractor_exiv2_la_LIBADD = \
  -lexiv2

libextractor_flac_la_SOURCES = \
  flac_extractor.c
libextractor_flac_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_flac_la_LIBADD = \
  -lFLAC $(flacoggflag) \
  $(LE_LIBINTL)

libextractor_flv_la_SOURCES = \
  flv_extractor.c
libextractor_flv_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_flv_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_gif_la_SOURCES = \
  gif_extractor.c 
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_gif_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_html_la_SOURCES = \
  html_extractor.c 
libextractor_html_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_html_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

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

libextractor_id3v23_la_SOURCES = \
  id3v23_extractor.c 
libextractor_id3v23_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v23_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_id3v24_la_SOURCES = \
  id3v24_extractor.c 
libextractor_id3v24_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v24_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_it_la_SOURCES = \
  it_extractor.c 
libextractor_it_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_jpeg_la_SOURCES = \
  jpeg_extractor.c 
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_jpeg_la_LIBADD = \
  $(LE_LIBINTL)

libextractor_man_la_SOURCES = \
  man_extractor.c 
libextractor_man_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_man_la_LIBADD = \
  $(LE_LIBINTL)

libextractor_mime_la_SOURCES = \
  mime_extractor.c 
libextractor_mime_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_mkv_la_SOURCES = \
  mkv_extractor.c 
libextractor_mkv_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_mp3_la_SOURCES = \
  mp3_extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mp3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(LE_LIBINTL)

libextractor_mpeg_la_SOURCES = \
  mpeg_extractor.c 
libextractor_mpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mpeg_la_LIBADD = \
  -lmpeg2

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
 $(top_builddir)/src/common/libextractor_common.la \
 -lz 

libextractor_ogg_la_SOURCES = \
  ogg_extractor.c
libextractor_ogg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ogg_la_LIBADD = \
  -lvorbisfile $(vorbisflag) -logg

libextractor_ole2_la_SOURCES =  \
  ole2_extractor.c
libextractor_ole2_la_CFLAGS = \
  $(GSF_CFLAGS) 
libextractor_ole2_la_LIBADD = \
  $(LIBADD) $(GSF_LIBS) \
  $(top_builddir)/src/common/libextractor_common.la 
libextractor_ole2_la_LDFLAGS = \
  $(PLUGINFLAGS) 

libextractor_pdf_la_SOURCES = \
  pdf_extractor.cc 
libextractor_pdf_la_LDFLAGS = \
  $(XTRA_CPPLIBS) $(PLUGINFLAGS) 
libextractor_pdf_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  -lpoppler

libextractor_png_la_SOURCES = \
  png_extractor.c
libextractor_png_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  -lz

libextractor_ps_la_SOURCES = \
  ps_extractor.c
libextractor_ps_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_qt_la_SOURCES = \
  qt_extractor.c 
libextractor_qt_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_qt_la_LIBADD = \
  -lz -lm

libextractor_real_la_SOURCES = \
  real_extractor.c 
libextractor_real_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_riff_la_SOURCES = \
  riff_extractor.c 
libextractor_riff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_riff_la_LIBADD = \
  $(LE_LIBINTL) \
  -lm 

libextractor_rpm_la_SOURCES = \
  rpm_extractor.c 
libextractor_rpm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_rpm_la_LIBADD = \
  -lrpm

libextractor_s3m_la_SOURCES = \
  s3m_extractor.c 
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_sid_la_SOURCES = \
  sid_extractor.c 
libextractor_sid_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_tar_la_SOURCES = \
  tar_extractor.c 
libextractor_tar_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_thumbnailffmpeg_la_SOURCES = \
  thumbnailffmpeg_extractor.c
libextractor_thumbnailffmpeg_la_LIBADD = \
  -lavformat -lavcodec -lswscale -lavutil -lz -lbz2
libextractor_thumbnailffmpeg_la_LDFLAGS = \
  $(PLUGINFLAGS) 

libextractor_thumbnailgtk_la_CFLAGS = \
  $(GLIB_CFLAGS) $(GTK_CFLAGS)
libextractor_thumbnailgtk_la_LIBADD = \
  $(LIBADD) -lgobject-2.0 @GTK_LIBS@ 
libextractor_thumbnailgtk_la_LDFLAGS = \
  $(PLUGINFLAGS) 
libextractor_thumbnailgtk_la_SOURCES = \
  thumbnailgtk_extractor.c

libextractor_thumbnailqt_la_SOURCES = \
  thumbnailqt_extractor.cc
libextractor_thumbnailqt_la_LDFLAGS = \
  $(QT_LDFLAGS) \
  $(PLUGINFLAGS)
libextractor_thumbnailqt_la_LIBADD = \
  $(qtflags) $(svgflags) 
libextractor_thumbnailqt_la_CPPFLAGS = \
  $(QT_CPPFLAGS) \
  $(QT_CFLAGS) $(QT_SVG_CFLAGS)

libextractor_tiff_la_SOURCES = \
  tiff_extractor.c 
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_tiff_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_wav_la_SOURCES = \
  wav_extractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_wav_la_LIBADD = \
  $(LE_LIBINTL)

libextractor_xm_la_SOURCES = \
  xm_extractor.c 
libextractor_xm_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_zip_la_SOURCES = \
  zip_extractor.c
libextractor_zip_la_LDFLAGS = \
  $(PLUGINFLAGS)

EXTRA_DIST = template_extractor.c 
