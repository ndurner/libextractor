include ../Makefile-plugins.am
include ffmpeg_distfiles.am

plugin_LTLIBRARIES = \
 libextractor_thumbnailffmpeg.la

libextractor_thumbnailffmpeg_la_SOURCES = \
  thumbnailextractorffmpeg.c 
libextractor_thumbnailffmpeg_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  ./ffmpeg/libavformat/libavformat.a \
  ./ffmpeg/libavcodec/libavcodec.a \
  ./ffmpeg/libavutil/libavutil.a \
  ./ffmpeg/libswscale/libswscale.a \
  -lz -lbz2
libextractor_thumbnailffmpeg_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_thumbnailffmpeg_la_CPPFLAGS = \
  -I$(top_scrdir)/include \
  -I./ffmpeg \
  -I./ffmpeg/libavformat \
  -I./ffmpeg/libavcodec \
  -I./ffmpeg/libavutil \
  -I./ffmpeg/libswscale

./ffmpeg/libavformat/libavformat.a: Makefile
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) all
./ffmpeg/libavcodec/libavcodec.a: Makefile
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) all
./ffmpeg/libavutil/libavutil.a: Makefile
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) all
./ffmpeg/libswscale/libswscale.a: Makefile
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) all

clean-local:
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) clean
distclean-local:
	cd ffmpeg && $(MAKE) $(AM_MAKEFLAGS) distclean

dist-hook: clean-local


