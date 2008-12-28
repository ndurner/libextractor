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

if HAVE_GLIB
if WITH_GSF
 GSF_LIBS_X = $(GSF_LIBS)
endif
endif

libextractor_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag) $(zlib) $(bz2lib) $(GSF_LIBS_X) $(LIBICONV) -lpthread


EXTRA_DIST = \
  winproc.c \
  iconv.c

if MINGW
  winproc = winproc.c
endif

libextractor_la_CPPFLAGS = -DPLUGINDIR=\"@RPLUGINDIR@\" $(AM_CPPFLAGS)

libextractor_la_SOURCES = \
  extractor.c \
  $(winproc) 

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 


check_PROGRAMS = \
  test_binary

TESTS = $(check_PROGRAMS)

test_binary_SOURCES = \
  test_binary.c
test_binary_LDADD = \
  $(top_builddir)/src/main/libextractor.la



