INCLUDES = -I$(top_srcdir)/src/include

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

if HAVE_GLIB
 oledir=ole2
endif

if HAVE_WAND
 thumbdir=thumbnail
endif

SUBDIRS = . oo printable hash $(oledir) rpm pdf $(thumbdir)

PLUGINFLAGS = -export-dynamic -avoid-version -module


if HAVE_VORBISFILE
 extraogg = libextractor_ogg.la
endif

if HAVE_ZLIB
extraqt = libextractor_qt.la
endif

plugin_LTLIBRARIES = \
  libextractor_id3v24.la \
  libextractor_id3v23.la \
  libextractor_tar.la \
  libextractor_deb.la \
  libextractor_man.la \
  libextractor_wav.la \
  libextractor_elf.la \
  libextractor_asf.la \
  libextractor_dvi.la \
  libextractor_id3v2.la \
  libextractor_jpeg.la \
  libextractor_html.la \
  libextractor_png.la \
  libextractor_gif.la \
  libextractor_tiff.la \
  libextractor_real.la \
  libextractor_filename.la \
  libextractor_split.la \
  libextractor_mp3.la \
  libextractor_mime.la \
  libextractor_riff.la \
  libextractor_mpeg.la \
  libextractor_ps.la \
  libextractor_zip.la \
  libextractor_lower.la \
  libextractor_translit.la \
  $(extraqt) \
  $(extraogg) 

if HAVE_VORBISFILE
libextractor_ogg_la_SOURCES = \
  oggextractor.c
libextractor_ogg_la_LDFLAGS = \
 -lvorbisfile $(PLUGINFLAGS)
endif

noinst_LTLIBRARIES = \
  libpack.la \
  libconvert.la


if HAVE_ZLIB
libextractor_qt_la_LDFLAGS = \
  -lz $(PLUGINFLAGS)
libextractor_qt_la_SOURCES = \
  qtextractor.c 
endif

libextractor_ps_la_SOURCES = \
  psextractor.c
libextractor_ps_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)

libextractor_wav_la_SOURCES = \
  wavextractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS) 

libextractor_mp3_la_SOURCES = \
  mp3extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS) 
libextractor_mp3_la_LIBADD = \
  libconvert.la

libextractor_id3v2_la_SOURCES = \
  id3v2extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v2_la_LIBADD = \
  libconvert.la

libextractor_id3v23_la_SOURCES = \
  id3v23extractor.c 
libextractor_id3v23_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v23_la_LIBADD = \
  libconvert.la

libextractor_id3v24_la_SOURCES = \
  id3v24extractor.c 
libextractor_id3v24_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_id3v24_la_LIBADD = \
  libconvert.la

libextractor_dvi_la_SOURCES = \
  dviextractor.c 
libextractor_dvi_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_asf_la_SOURCES = \
  asfextractor.c 
libextractor_asf_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_tar_la_SOURCES = \
  tarextractor.c 
libextractor_tar_la_LDFLAGS = \
  $(PLUGINFLAGS) -lz

libextractor_lower_la_SOURCES = \
  lowerextractor.c
libextractor_lower_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_gif_la_SOURCES = \
  gifextractor.c 
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_gif_la_LIBADD = \
  libpack.la

libextractor_tiff_la_SOURCES = \
  tiffextractor.c 
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_tiff_la_LIBADD = \
  libpack.la

libextractor_elf_la_SOURCES = \
  elfextractor.c 
libextractor_elf_la_LDFLAGS = \
  $(PLUGINFLAGS)
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

libextractor_zip_la_SOURCES = \
  zipextractor.c
libextractor_zip_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_jpeg_la_SOURCES = \
  jpegextractor.c 
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS) 

libextractor_html_la_SOURCES = \
  htmlextractor.c 
libextractor_html_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) 

libextractor_real_la_SOURCES = \
  realextractor.c 
libextractor_real_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_mime_la_SOURCES = \
  mimeextractor.c 
libextractor_mime_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)

libextractor_mpeg_la_SOURCES = \
  mpegextractor.c 
libextractor_mpeg_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) -lm

libextractor_man_la_SOURCES = \
  manextractor.c 
libextractor_man_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) -lz 

libextractor_deb_la_SOURCES = \
  debextractor.c 
libextractor_deb_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) -lz -lpthread

libextractor_riff_la_SOURCES = \
  riffextractor.c 
libextractor_riff_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la -lm \
  $(PLUGINFLAGS) 

libextractor_png_la_SOURCES = \
  pngextractor.c
libextractor_png_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)
libextractor_png_la_LIBADD = \
  libconvert.la -lz

libextractor_filename_la_SOURCES = \
  filenameextractor.c 
libextractor_filename_la_LDFLAGS = \
  $(PLUGINFLAGS)
libextractor_filename_la_LIBADD = \
  libconvert.la

libextractor_split_la_SOURCES = \
  splitextractor.c 
libextractor_split_la_LDFLAGS = \
  $(PLUGINFLAGS)

libextractor_translit_la_SOURCES = \
  translitextractor.c 
libextractor_translit_la_LDFLAGS = \
  $(PLUGINFLAGS)

