SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

LIBS = \
 @LE_LIBINTL@ @LE_LIB_LIBS@

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

if HAVE_ZLIB
zlib =-lz
TEST_ZLIB = test_gzip
endif
if HAVE_BZ2
bz2lib = -lbz2
TEST_BZIP2 = test_bzip2
endif


if WINDOWS
EXTRACTOR_IPC=extractor_ipc_w32.c
else
EXTRACTOR_IPC=extractor_ipc_gnu.c
endif

if HAVE_GNU_LD
 makesymbolic=-Wl,-Bsymbolic
endif

PLUGINFLAGS = $(makesymbolic) $(LE_PLUGIN_LDFLAGS)

EXTRA_DIST = \
  iconv.c \
  test_file.dat \
  test_file.dat.gz \
  test_file.dat.bz2

libextractor_la_CPPFLAGS = \
  -DPLUGINDIR=\"@RPLUGINDIR@\" -DPLUGININSTDIR=\"${plugindir}\" $(AM_CPPFLAGS)
libextractor_la_SOURCES = \
  extractor_common.c extractor_common.h \
  extractor_datasource.c extractor_datasource.h \
  $(EXTRACTOR_IPC) extractor_ipc.c extractor_ipc.h \
  extractor_logging.c extractor_logging.h \
  extractor_metatypes.c \
  extractor_plugpath.c extractor_plugpath.h \
  extractor_plugins.c extractor_plugins.h \
  extractor_print.c \
  extractor_plugin_main.c extractor_plugin_main.h \
  extractor.c
libextractor_la_LDFLAGS = \
  $(LE_LIB_LDFLAGS) -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@
libextractor_la_LIBADD = \
  $(LIBLTDL) $(zlib) $(bz2lib) $(LTLIBICONV) $(XLIB) $(LE_LIBINTL)

extract_SOURCES = \
  extract.c \
  getopt.c getopt.h getopt1.c 
extract_DEPENDENCIES = \
  libextractor.la
extract_LDADD = \
 $(top_builddir)/src/main/libextractor.la $(LE_LIBINTL)

TESTS_ENVIRONMENT = testdatadir=$(top_srcdir)/test
TESTS_ENVIRONMENT += bindir=${bindir}

noinst_LTLIBRARIES = \
  libextractor_test.la \
  libextractor_test2.la 

libextractor_test_la_SOURCES = \
  test_extractor.c 
libextractor_test_la_LDFLAGS = \
  $(PLUGINFLAGS) -rpath /nowhere
libextractor_test_la_LIBADD = \
  $(LE_LIBINTL) $(XLIB)

libextractor_test2_la_SOURCES = \
  test2_extractor.c 
libextractor_test2_la_LDFLAGS = \
  $(PLUGINFLAGS) -rpath /nowhere
libextractor_test2_la_LIBADD = \
  $(LE_LIBINTL) $(XLIB)


check_PROGRAMS = \
 test_trivial \
 test_plugin_loading \
 test_plugin_load_multi \
 test_ipc \
 test_file \
 $(TEST_ZLIB) \
 $(TEST_BZIP2)

if ENABLE_TEST_RUN
TESTS = $(check_PROGRAMS)
endif

test_trivial_SOURCES = \
 test_trivial.c 
test_trivial_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_plugin_loading_SOURCES = \
 test_plugin_loading.c 
test_plugin_loading_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_plugin_load_multi_SOURCES = \
 test_plugin_load_multi.c 
test_plugin_load_multi_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_ipc_SOURCES = \
 test_ipc.c 
test_ipc_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_file_SOURCES = \
 test_file.c 
test_file_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_gzip_SOURCES = \
 test_gzip.c 
test_gzip_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

test_bzip2_SOURCES = \
 test_bzip2.c 
test_bzip2_LDADD = \
 $(top_builddir)/src/main/libextractor.la  

