SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/@RPLUGINDIR@

LIBS = \
 @LE_LIBINTL@ @LE_LIB_LIBS@

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
  $(LIBLTDL) $(zlib) $(bz2lib) $(LTLIBICONV)

if WINDOWS
EXTRACTOR_IPC=extractor_ipc_w32.c
else
EXTRACTOR_IPC=extractor_ipc_gnu.c
endif

EXTRA_DIST = \
  iconv.c fuzz_default.sh fuzz_thumbnail.sh

libextractor_la_CPPFLAGS = -DPLUGINDIR=\"@RPLUGINDIR@\" -DPLUGININSTDIR=\"${plugindir}\" $(AM_CPPFLAGS)

libextractor_la_SOURCES = \
  extractor_metatypes.c \
  extractor_print.c \
  extractor_plugpath.c extractor_plugpath.h \
  extractor_plugins.c extractor_plugins.h \
  $(EXTRACTOR_IPC) extractor_ipc.c extractor_ipc.h \
  extractor_plugin_main.c extractor_plugin_main.h \
  extractor.c 

extract_SOURCES = \
  extract.c \
  getopt.c getopt.h getopt1.c 


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

TESTS = $(check_PROGRAMS) $(fuzz_tests)

test_trivial_SOURCES = \
 test_trivial.c 

test_plugin_loading_SOURCES = \
 test_plugin_loading.c 

test_plugin_load_multi_SOURCES = \
 test_plugin_load_multi.c 

