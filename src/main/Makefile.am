SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

AM_CFLAGS = \
  -DPLUGIN_PATH="\"${plugindir}\""

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

if !MINGW
  dlflag=-ldl
endif

libextractor_la_LDFLAGS = \
  -export-dynamic -version-info 1:1:0 $(LIBICONV)
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag)
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

