SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

LIBS = \
 @LE_LIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

if !MINGW
if !SOMEBSD
    dlflag=-ldl
endif
endif

if HAVE_ZLIB
  zlib =-lz
endif
if HAVE_BZ2
  bz2lib = -lbz2
endif

libextractor_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag) $(zlib) $(bz2lib) $(LIBICONV) -lrt


EXTRA_DIST = \
  iconv.c

libextractor_la_CPPFLAGS = -DPLUGINDIR=\"@RPLUGINDIR@\" $(AM_CPPFLAGS)

libextractor_la_SOURCES = \
  extractor.c \
  extractor_metatypes.c \
  extractor_print.c 

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 


