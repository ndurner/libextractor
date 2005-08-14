INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

PLUGINFLAGS = -export-dynamic -avoid-version -module

plugin_LTLIBRARIES = \
  libextractor_ole2.la

AM_CFLAGS = $(GLIB_CFLAGS)

libextractor_ole2_la_CFLAGS = \
  $(GLIB_CFLAGS)
libextractor_ole2_la_LIBADD = \
  $(LIBADD) $(GLIB_LIBS) -lgobject-2.0 \
  $(top_builddir)/src/main/libextractor.la
libextractor_ole2_la_LDFLAGS = \
  -export-dynamic -avoid-version -module
# Note that we're NOT using PLUGINFLAGS here to avoid -export-dynamic!
libextractor_ole2_la_SOURCES =  \
        ole2extractor.c         \
	gsf-utils.c		\
	gsf-utils.h		\
	gsf-input.c		\
	gsf-input.h		\
	gsf-infile-msole.c 	\
	gsf-infile-msole.h 	
