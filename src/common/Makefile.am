INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

LIBS = \
 @LTLIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor_common.la

if !MINGW
  exportflag=-export-symbols-regex "EXTRACTOR_.*"
if !SOMEBSD
    dlflag=-ldl
endif
else
  winexportflag=-export-dynamic
endif

libextractor_common_la_LDFLAGS = \
  $(exportflag) $(winexportflag) -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@ $(LIBICONV)

libextractor_common_la_LIBADD = \
  $(LIBLTDL) $(dlflag)

libextractor_common_la_SOURCES = \
  pack.c \
  pack.h \
  convert.c \
  convert.h \
  convert_numeric.c \
  convert_numeric.h
