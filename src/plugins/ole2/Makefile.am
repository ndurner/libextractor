INCLUDES = -I$(top_srcdir)/src/include

LIBS = \
 @LTLIBINTL@ @LIBS@

# install plugins under:
plugindir = $(libdir)/libextractor

PLUGINFLAGS = -export-dynamic -avoid-version -module

plugin_LTLIBRARIES = \
  libextractor_ole2.la

AM_CFLAGS = $(GLIB_CFLAGS)

if MINGW
  WINFLAGS = -lgobject-2.0 
endif

libextractor_ole2_la_CFLAGS = \
  $(GLIB_CFLAGS)
libextractor_ole2_la_LIBADD = \
  $(LIBADD) $(GLIB_LIBS) $(WINFLAGS) \
  $(top_builddir)/src/main/libextractor.la
# Ok, linking this one is complicated, see Mantis #787.
libextractor_ole2_la_LDFLAGS = \
  -Wl,-Bstatic -Wl,-lgobject-2.0 -Wl,-lglib-2.0 -Wl,-Bdynamic \
  -Wl,-Bsymbolic -avoid-version -module
# Note that we're NOT using PLUGINFLAGS here to avoid -export-dynamic!
libextractor_ole2_la_SOURCES =  \
        ole2extractor.c         \
	gsf.h			\
	gsf-utils.c		\
	gsf-utils.h		\
	gsf-impl-utils.h	\
	gsf-input.c		\
	gsf-input.h		\
	gsf-input-impl.h	\
	gsf-input-memory.c 	\
	gsf-input-memory.h 	\
	gsf-infile.h 		\
	gsf-infile.c 		\
	gsf-infile-impl.h 	\
	gsf-infile-msole.c 	\
	gsf-infile-msole.h 	\
	gsf-msole-impl.h	\
	gsf-shared-memory.c	\
	gsf-shared-memory.h	
