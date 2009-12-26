SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

LIBS = \
 @LE_LIBINTL@ @LE_EXTRACTOR_LIBS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

if HAVE_ZLIB
  zlib =-lz
endif
if HAVE_BZ2
  bz2lib = -lbz2
endif

libextractor_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@
libextractor_la_LIBADD = \
  $(LIBLTDL) $(zlib) $(bz2lib) $(LIBICONV)


EXTRA_DIST = \
  iconv.c

libextractor_la_CPPFLAGS = -DPLUGINDIR=\"@RPLUGINDIR@\" -DPLUGININSTDIR=\"${plugindir}\" $(AM_CPPFLAGS)

libextractor_la_SOURCES = \
  extractor.c \
  extractor_metatypes.c \
  extractor_print.c 

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 




ELIBDIR = $(libdir)

LDADD = \
 $(top_builddir)/src/main/libextractor.la  

if HAVE_ZZUF
  fuzz_tests=fuzz_default.sh fuzz_thumbnail.sh
endif

TESTS_ENVIRONMENT = testdatadir=$(top_srcdir)/test
TESTS_ENVIRONMENT += bindir=${bindir}

check_PROGRAMS = \
 test_trivial \
 test_plugin_loading \
 test_plugin_load_multi

nocheck = \
 mt_plugintest1 \
 mt_plugintest2 \
 mt_extracttest1 \
 mt_extracttest2

TESTS = $(check_PROGRAMS) $(fuzz_tests)

test_trivial_SOURCES = \
 test_trivial.c 

test_plugin_loading_SOURCES = \
 test_plugin_loading.c 

test_plugin_load_multi_SOURCES = \
 test_plugin_load_multi.c 

mt_plugintest1_SOURCES = \
 mt_plugintest1.c 
mt_plugintest1_LDFLAGS = \
 -lpthread

mt_plugintest2_SOURCES = \
 mt_plugintest2.c 
mt_plugintest2_LDFLAGS = \
 -lpthread

mt_extracttest1_SOURCES = \
 mt_extracttest1.c 
mt_extracttest1_LDFLAGS = \
 -lpthread

mt_extracttest2_SOURCES = \
 mt_extracttest2.c 
mt_extracttest2_LDFLAGS = \
 -lpthread


