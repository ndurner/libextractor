include Makefile-plugins.am
include ffmpeg_distfiles.am

LIBEXT = @LIBEXT@

if HAVE_FFMPEG
thumbffmpeg=thumbnailffmpeg
endif

if HAVE_LIBRPM
rpm=rpm
endif

if HAVE_GLIB
if WITH_GSF
 oledir=ole2
endif
if HAVE_GTK
thumbgtk=thumbnail
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

if HAVE_PRINTABLE
 printdir=printable
endif

if HAVE_CXX
if HAVE_EXIV2
 exiv2dir=exiv2
endif
endif


if HAVE_XPDF
 xpdfdir=pdf
else
 pdfplugin=libextractor_pdf.la 
endif

if HAVE_MPEG2
 extrampeg = libextractor_mpeg.la
endif

# toggle for development
# SUBDIRS = . 
SUBDIRS = $(thumbgtk) $(thumbffmpeg) . $(oodir) $(printdir) hash $(oledir) $(rpm) $(xpdfdir) $(exiv2dir) 


if HAVE_VORBISFILE
 extraogg = libextractor_ogg.la
endif

if HAVE_FLAC
 extraflac = libextractor_flac.la
endif

if NEED_VORBIS
 vorbisflag = -lvorbis
endif

if NEED_OGG
 flacoggflag = -logg
endif

if HAVE_ZLIB
extraqt = libextractor_qt.la
oodir = oo
endif

plugin_LTLIBRARIES = $(pdfplugin) \
  libextractor_applefile.la \
  libextractor_asf.la \
  libextractor_deb.la \
  libextractor_dvi.la \
  libextractor_elf.la \
  libextractor_filename.la \
  $(extraflac) \
  libextractor_flv.la \
  libextractor_gif.la \
  libextractor_html.la \
  libextractor_id3v2.la \
  libextractor_id3v24.la \
  libextractor_id3v23.la \
  libextractor_it.la \
  libextractor_jpeg.la \
  libextractor_lower.la \
  libextractor_man.la \
  libextractor_mime.la \
  libextractor_mp3.la \
  $(extrampeg) \
  libextractor_nsf.la \
  libextractor_nsfe.la \
  $(extraogg) \
  libextractor_png.la \
  libextractor_ps.la \
  $(extraqt) \
  libextractor_real.la \
  libextractor_riff.la \
  libextractor_s3m.la \
  libextractor_sid.la \
  libextractor_split.la \
  libextractor_tar.la \
  libextractor_tiff.la \
  $(thumbqt) \
  libextractor_translit.la \
  libextractor_wav.la \
  libextractor_xm.la \
  libextractor_zip.la 

if HAVE_VORBISFILE
libextractor_ogg_la_SOURCES = \
  oggextractor.c
libextractor_ogg_la_LDFLAGS = \
 -lvorbisfile $(vorbisflag) -logg $(PLUGINFLAGS)  $(retaincommand)
endif


if HAVE_FLAC
libextractor_flac_la_SOURCES = \
  flacextractor.c
libextractor_flac_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_flac_la_LDFLAGS = \
 -lFLAC $(flacoggflag) $(PLUGINFLAGS)  $(retaincommand)
endif

noinst_LTLIBRARIES = \
  libpack.la \
  libconvert.la \
  libconvert_numeric.la

if HAVE_ZLIB
libextractor_qt_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_qt_la_LIBADD = \
  -lz
libextractor_qt_la_SOURCES = \
  qtextractor.c 
endif

libextractor_ps_la_SOURCES = \
  psextractor.c
libextractor_ps_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_wav_la_SOURCES = \
  wavextractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_pdf_la_SOURCES = \
  pdfextractor.c
libextractor_pdf_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_pdf_la_LIBADD = \
  libconvert.la  

libextractor_mp3_la_SOURCES = \
  mp3extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_mp3_la_LIBADD = \
  libconvert.la  

libextractor_id3v2_la_SOURCES = \
  id3v2extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v2_la_LIBADD = \
  libconvert.la

libextractor_id3v23_la_SOURCES = \
  id3v23extractor.c 
libextractor_id3v23_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v23_la_LIBADD = \
  libconvert.la 

libextractor_id3v24_la_SOURCES = \
  id3v24extractor.c 
libextractor_id3v24_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v24_la_LIBADD = \
  libconvert.la

libextractor_it_la_SOURCES = \
  itextractor.c 
libextractor_it_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_it_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_dvi_la_SOURCES = \
  dviextractor.c 
libextractor_dvi_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_asf_la_SOURCES = \
  asfextractor.c 
libextractor_asf_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

if HAVE_ZLIB
libextractor_tar_la_SOURCES = \
  tarextractor.c 
libextractor_tar_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_tar_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la -lz
endif

libextractor_lower_la_SOURCES = \
  lowerextractor.c
libextractor_lower_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_gif_la_SOURCES = \
  gifextractor.c 
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_gif_la_LIBADD = \
  libpack.la

libextractor_tiff_la_SOURCES = \
  tiffextractor.c 
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_tiff_la_LIBADD = \
  libpack.la

libextractor_applefile_la_SOURCES = \
  applefileextractor.c
libextractor_applefile_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_applefile_la_LIBADD = \
  libpack.la

libextractor_elf_la_SOURCES = \
  elfextractor.c 
libextractor_elf_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_elf_la_LIBADD = \
  libpack.la

libpack_la_SOURCES = \
  pack.c \
  pack.h 

libconvert_la_SOURCES = \
  convert.c \
  convert.h 
libconvert_la_LDFLAGS = \
  $(LIBICONV)

libconvert_numeric_la_SOURCES = \
  convert_numeric.c \
  convert_numeric.h 

libextractor_zip_la_SOURCES = \
  zipextractor.c
libextractor_zip_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_jpeg_la_SOURCES = \
  jpegextractor.c 
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)   $(retaincommand)

libextractor_html_la_SOURCES = \
  htmlextractor.c 
libextractor_html_la_LDFLAGS = \
  $(PLUGINFLAGS)   $(retaincommand)
libextractor_html_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  libconvert.la 

libextractor_flv_la_SOURCES = \
  flvextractor.c
libextractor_flv_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_flv_la_LIBADD = \
  libconvert_numeric.la

libextractor_real_la_SOURCES = \
  realextractor.c 
libextractor_real_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_mime_la_SOURCES = \
  mimeextractor.c 
libextractor_mime_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

if HAVE_MPEG2
libextractor_mpeg_la_SOURCES = \
  mpegextractor.c 
libextractor_mpeg_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_mpeg_la_LIBADD = \
  -lmpeg2
endif

libextractor_man_la_SOURCES = \
  manextractor.c 
libextractor_man_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)

if HAVE_ZLIB
libextractor_deb_la_SOURCES = \
  debextractor.c 
libextractor_deb_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_deb_la_LIBADD = \
  -lz
endif

libextractor_riff_la_SOURCES = \
  riffextractor.c 
libextractor_riff_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_riff_la_LIBADD = \
  -lm 

if HAVE_ZLIB
libextractor_png_la_SOURCES = \
  pngextractor.c
libextractor_png_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_png_la_LIBADD = \
  libconvert.la -lz
endif

libextractor_filename_la_SOURCES = \
  filenameextractor.c 
libextractor_filename_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_filename_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  libconvert.la

libextractor_sid_la_SOURCES = \
  sidextractor.c 
libextractor_sid_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_sid_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_nsf_la_SOURCES = \
  nsfextractor.c 
libextractor_nsf_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_nsf_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_nsfe_la_SOURCES = \
  nsfeextractor.c 
libextractor_nsfe_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_nsfe_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_xm_la_SOURCES = \
  xmextractor.c 
libextractor_xm_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_xm_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_s3m_la_SOURCES = \
  s3mextractor.c 
libextractor_s3m_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_s3m_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la

libextractor_split_la_SOURCES = \
  splitextractor.c 
libextractor_split_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

libextractor_translit_la_SOURCES = \
  translitextractor.c 
libextractor_translit_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

libextractor_thumbnailqt_la_SOURCES = \
	thumbnailextractorqt.cc
libextractor_thumbnailqt_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_thumbnailqt_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand) $(qtflags) $(svgflags)
libextractor_thumbnailqt_la_CPPFLAGS = \
  -I$(top_scrdir)/include \
  $(QT_CFLAGS) $(QT_SVG_CFLAGS)

install-exec-hook:
	mkdir -p $(DESTDIR)$(plugindir) &> /dev/null || true
	rm -f $(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT)
	rm -f $(DESTDIR)$(plugindir)/libextractor_thumbnail.la
	if test "$(thumbffmpeg)" != ""; then \
		$(LN_S) $(plugindir)/libextractor_thumbnailffmpeg$(LIBEXT) \
			$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
	else \
		if test "$(thumbgtk)" != ""; then \
			$(LN_S) $(plugindir)/libextractor_thumbnailgtk$(LIBEXT) \
				$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
		else \
			if test "$(thumbqt)" != ""; then \
				$(LN_S) $(plugindir)/libextractor_thumbnailqt$(LIBEXT) \
					$(DESTDIR)$(plugindir)/libextractor_thumbnail$(LIBEXT); \
			fi \
		fi \
	fi

