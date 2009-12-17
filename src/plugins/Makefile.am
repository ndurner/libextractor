include Makefile-plugins.am

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
  libextractor_gif.la \
  libextractor_html.la \
  libextractor_it.la \
  libextractor_jpeg.la \
  libextractor_man.la \
  libextractor_mime.la \
  libextractor_mp3.la \
  libextractor_odf.la \
  $(ogg) \
  $(ole2) \
  $(pdf) \
  $(rpm) \
  $(thumbgtk)

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

libextractor_mp3_la_SOURCES = \
  mp3_extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mp3_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(LE_LIBINTL)

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

libextractor_rpm_la_SOURCES = \
  rpm_extractor.c 
libextractor_rpm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_rpm_la_LIBADD = \
  -lrpm

libextractor_thumbnailgtk_la_CFLAGS = \
  $(GLIB_CFLAGS) $(GTK_CFLAGS)
libextractor_thumbnailgtk_la_LIBADD = \
  $(LIBADD) -lgobject-2.0 @GTK_LIBS@ 
libextractor_thumbnailgtk_la_LDFLAGS = \
  $(PLUGINFLAGS) 
libextractor_thumbnailgtk_la_SOURCES = \
  thumbnailgtk_extractor.c




# stuff below still needs to be ported to 0.6.x

# SUBDIRS = . $(thumbffmpeg) hash


OLD_LIBS = \
  libextractor_flv.la \
  libextractor_id3v2.la \
  libextractor_id3v24.la \
  libextractor_id3v23.la \
  $(extrampeg) \
  libextractor_nsf.la \
  libextractor_nsfe.la \
  libextractor_png.la \
  libextractor_ps.la \
  $(extraqt) \
  libextractor_real.la \
  libextractor_riff.la \
  libextractor_s3m.la \
  libextractor_sid.la \
  libextractor_tar.la \
  libextractor_tiff.la \
  $(thumbqt) \
  libextractor_wav.la \
  libextractor_xm.la \
  libextractor_zip.la 

if HAVE_ZLIB
libextractor_qt_la_SOURCES = \
  qtextractor.c 
libextractor_qt_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_qt_la_LIBADD = \
  -lz
endif

libextractor_ps_la_SOURCES = \
  psextractor.c
libextractor_ps_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_ps_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_wav_la_SOURCES = \
  wavextractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_wav_la_LIBADD = \
  $(LE_LIBINTL)

libextractor_id3v2_la_SOURCES = \
  id3v2extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v2_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_id3v23_la_SOURCES = \
  id3v23extractor.c 
libextractor_id3v23_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v23_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_id3v24_la_SOURCES = \
  id3v24extractor.c 
libextractor_id3v24_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v24_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_tar_la_SOURCES = \
  tarextractor.c 
libextractor_tar_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_tar_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la -lz

libextractor_tiff_la_SOURCES = \
  tiffextractor.c 
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_tiff_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_zip_la_SOURCES = \
  zipextractor.c
libextractor_zip_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_zip_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_flv_la_SOURCES = \
  flvextractor.c
libextractor_flv_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_flv_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la

libextractor_real_la_SOURCES = \
  realextractor.c 
libextractor_real_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_mpeg_la_SOURCES = \
  mpegextractor.c 
libextractor_mpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_mpeg_la_LIBADD = \
  -lmpeg2 \
  $(top_builddir)/src/main/libextractor.la

libextractor_riff_la_SOURCES = \
  riffextractor.c 
libextractor_riff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_riff_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(LE_LIBINTL) \
  -lm 

libextractor_png_la_SOURCES = \
  pngextractor.c
libextractor_png_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  $(top_builddir)/src/common/libextractor_common.la \
  $(top_builddir)/src/main/libextractor.la \
  -lz

libextractor_sid_la_SOURCES = \
  sidextractor.c 
libextractor_sid_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_sid_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_nsf_la_SOURCES = \
  nsfextractor.c 
libextractor_nsf_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_nsf_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_nsfe_la_SOURCES = \
  nsfeextractor.c 
libextractor_nsfe_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_nsfe_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_xm_la_SOURCES = \
  xmextractor.c 
libextractor_xm_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_xm_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_s3m_la_SOURCES = \
  s3mextractor.c 
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_s3m_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_thumbnailqt_la_SOURCES = \
  thumbnailextractorqt.cc
libextractor_thumbnailqt_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_thumbnailqt_la_LIBADD = \
  $(qtflags) $(svgflags) \
  $(top_builddir)/src/main/libextractor.la
libextractor_thumbnailqt_la_CPPFLAGS = \
  -I$(top_scrdir)/include \
  $(QT_CFLAGS) $(QT_SVG_CFLAGS)


# Fixme: is this still working ok? Doubtful!

install-exec-hook:
	mkdir -p $(DESTDIR)$(plugindir) &> /dev/null || true
	rm -f $(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT)
	rm -f $(DESTDIR)$(plugindir)/libextractor_thumbnail.la
	if test "$(thumbgtk)" != ""; then \
		$(LN_S) $(plugindir)/libextractor_thumbnailgtk$(LIBEXT) \
			$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
	else \
		if test "$(thumbqt)" != ""; then \
			$(LN_S) $(plugindir)/libextractor_thumbnailqt$(LIBEXT) \
				$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
		else \
			if test "$(thumbffmpeg)" != ""; then \
				$(LN_S) $(plugindir)/libextractor_thumbnailffmpeg$(LIBEXT) \
					$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
			fi \
		fi \
	fi


