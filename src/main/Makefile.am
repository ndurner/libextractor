SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

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
  -export-dynamic -version-info 2:1:1 $(LIBICONV)
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag) $(zlib) $(bz2lib) $(GSF_LIBS_X)
libextractor_la_DEPENDENCIES = \
  $(LIBLTDL) 


EXTRA_DIST = \
  winproc.c \
  iconv.c

if MINGW
  winproc = winproc.c
endif

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

